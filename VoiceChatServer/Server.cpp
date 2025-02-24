//
// Created by Amin on 10/15/23.
//
#include "Server.h"

Server* Server::Instance = nullptr;
SteamNetworkingMicroseconds Server::g_logTimeZero;
ISteamNetworkingSockets* Server::steamNetworking;
HSteamNetPollGroup Server::connectionPollGroup;


bool Server::StartServer(uint16 port) {
    Instance = this;

    // Create client and server sockets
    InitSteamDatagramConnectionSockets();

    steamNetworking = SteamNetworkingSockets();

    connectionPollGroup = steamNetworking->CreatePollGroup();

    if (steamNetworking == nullptr){
        printf("steam networking is null");
    }

    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.m_port = port;

    SteamNetworkingConfigValue_t options;
    options.SetPtr( k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*) OnSteamNetConnectionStatusChanged );
    socket = steamNetworking->CreateListenSocketIP(addr, 1, &options);

    if ( socket == k_HSteamListenSocket_Invalid ) {
        printf("Failed to listen on port %d", port);
        return false;
    }
    if ( socket == k_HSteamNetPollGroup_Invalid ) {
        printf("Failed to listen on port %d", port);
        return false;
    }
    printf( "Server listening on port %d\n", port );


    return true;
}

Server::~Server() {
    steamNetworking->DestroyPollGroup( connectionPollGroup );
    steamNetworking = nullptr;
    printf("server cleaned \n");
}

void Server::DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg )
{
    SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    printf( "%10.6f %s\n", time*1e-6, pszMsg );
    fflush(stdout);
    if ( eType == k_ESteamNetworkingSocketsDebugOutputType_Bug )
    {
        fflush(stdout);
        fflush(stderr);
    }
}

void Server::InitSteamDatagramConnectionSockets()
{
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
    SteamDatagramErrMsg errMsg;
    if ( !GameNetworkingSockets_Init( nullptr, errMsg ) )
        printf( "GameNetworkingSockets_Init failed.  %s", errMsg );
#else
    SteamDatagram_SetAppID( 570 ); // Just set something, doesn't matter what
		SteamDatagram_SetUniverse( false, k_EUniverseDev );

		SteamDatagramErrMsg errMsg;
		if ( !SteamDatagramClient_Init( errMsg ) )
			FatalError( "SteamDatagramClient_Init failed.  %s", errMsg );

		// Disable authentication when running with Steam, for this
		// example, since we're not a real app.
		//
		// Authentication is disabled automatically in the open-source
		// version since we don't have a trusted third party to issue
		// certs.
		SteamNetworkingUtils()->SetGlobalConfigValueInt32( k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1 );
#endif

    g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();

    SteamNetworkingUtils()->SetDebugOutputFunction( k_ESteamNetworkingSocketsDebugOutputType_Msg, DebugOutput );
}

void Server::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo) {
    char temp[1024];

    // What's the state of the connection?
    switch ( pInfo->m_info.m_eState )
    {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            // Ignore if they were not previously connected.  (If they disconnected
            // before we accepted the connection.)
            if ( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected )
            {

                // Locate the client.  Note that it should have been found, because this
                // is the only codepath where we remove clients (except on shutdown),
                // and connection change callbacks are dispatched in queue order.

                // Select appropriate log messages
                const char *pszDebugLogAction;
                if ( pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally )
                {
                    pszDebugLogAction = "problem detected locally";
                    sprintf( temp, "Alas, hath fallen into shadow.");
                }
                else
                {
                    // Note that here we could check the reason code to see if
                    // it was a "usual" connection or an "unusual" one.
                    pszDebugLogAction = "closed by peer";
                    //sprintf( temp, "%s hath departed", itClient->second.m_sNick.c_str() );
                }

                // Spew something to our own log.  Note that because we put their nick
                // as the connection description, it will show up, along with their
                // transport-specific data (e.g. their IP address)
                printf( "Connection %s %s, reason %d: %s\n",
                        pInfo->m_info.m_szConnectionDescription,
                        pszDebugLogAction,
                        pInfo->m_info.m_eEndReason,
                        pInfo->m_info.m_szEndDebug
                );

            }
            else
            {
                assert( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting );
            }

            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
        {
            // This must be a new connection


            printf( "Connection request from %s", pInfo->m_info.m_szConnectionDescription );

            // A client is attempting to connect
            // Try to accept the connection.
            if ( steamNetworking->AcceptConnection( pInfo->m_hConn ) != k_EResultOK )
            {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                printf( "Can't accept connection.  (It was already closed?)" );
                break;
            }

            // Assign the poll group
            if ( !steamNetworking->SetConnectionPollGroup( pInfo->m_hConn, connectionPollGroup ) )
            {
                steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                printf( "Failed to set poll group?" );
                break;
            }

            // Generate a random nick.  A random temporary nick
            // is really dumb and not how you would write a real chat server.
            // You would want them to have some sort of signon message,
            // and you would keep their client in a state of limbo (connected,
            // but not logged on) until them.  I'm trying to keep this example
            // code really simple.
            char nick[ 64 ];
            sprintf( nick, "BraveWarrior%d", 10000 + ( rand() % 100000 ) );

            // Send them a welcome message
            sprintf( temp, "Welcome, stranger.  Thou art known to us for now as '%s'; upon thine command '/nick' we shall know thee otherwise.", nick );



            // Let everybody else know who they are for now
            sprintf( temp, "Hark!  A stranger hath joined this merry host.  For now we shall call them '%s'", nick );

            break;
        }

        case k_ESteamNetworkingConnectionState_Connected:
            // We will get a callback immediately after accepting the connection.
            // Since we are the server, we can ignore this, it's not news to us.

            printf( "Client connected, %s", pInfo->m_info.m_szConnectionDescription );

            break;

        default:
            // Silences -Wswitch
            break;
    }
}

void Server::PollIncomingMessages() {
    char temp[ 1024 ];

    while ( 1)
    {
        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = steamNetworking->ReceiveMessagesOnPollGroup( connectionPollGroup, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            break;
        if ( numMsgs < 0 )
            printf( "Error checking for messages" );
        assert( numMsgs == 1 && pIncomingMsg );

        if (pIncomingMsg->m_pData == NULL){
            printf("received null data!\n");
            break;
        }

        uint8_t messageType = ((uint8_t*)pIncomingMsg->m_pData)[0];

        printf("message type is %d \n", messageType);


        switch (messageType)
        {
            case SET_TOPIC:
                // TODO
                break;
            case AUDIO:
                printf("size of received data: %u \n", pIncomingMsg->GetSize());
                steamNetworking->SendMessageToConnection(pIncomingMsg->m_conn, pIncomingMsg->m_pData, pIncomingMsg->GetSize(),
                    k_nSteamNetworkingSend_ReliableNoNagle,
                    nullptr);
                printf("Data received on server, data size = %u bytes\n", pIncomingMsg->GetSize());
                break;
            default:
                break;
        }

        // We don't need this anymore.
        pIncomingMsg->Release();


    }
}

void Server::PollConnectionStateChanges() {
    steamNetworking->RunCallbacks();
}
