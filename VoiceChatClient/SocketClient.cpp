//
// Created by Amin on 10/17/23.
//

#include "SocketClient.h"

SteamNetworkingMicroseconds SocketClient::g_logTimeZero;
HSteamNetConnection SocketClient::connection;
ISteamNetworkingSockets* SocketClient::steamNetworking;
SocketClient* SocketClient::Instance = nullptr;


void SocketClient::InitSteamDatagramConnectionSockets() {
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

void SocketClient::DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg) {
    SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    printf( "%10.6f %s\n", time*1e-6, pszMsg );
    fflush(stdout);
    if ( eType == k_ESteamNetworkingSocketsDebugOutputType_Bug )
    {
        fflush(stdout);
        fflush(stderr);
    }
}

void SocketClient::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo) {
    assert(pInfo->m_hConn == connection || connection == k_HSteamNetConnection_Invalid );

    // What's the state of the connection?
    switch ( pInfo->m_info.m_eState )
    {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            // Print an appropriate message
            if ( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting )
            {
                // Note: we could distinguish between a timeout, a rejected connection,
                // or some other transport problem.
                printf( "(%s)\n", pInfo->m_info.m_szEndDebug );
            }
            else if ( pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally )
            {
                printf( "(%s)\n", pInfo->m_info.m_szEndDebug );
            }
            else
            {
                // NOTE: We could check the reason code for a normal disconnection
                printf( "(%s)\n", pInfo->m_info.m_szEndDebug );
            }

            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            printf("closing connection\n");
            steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
            connection = k_HSteamNetConnection_Invalid;
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
            // We will get this callback when we start connecting.
            // We can ignore this.
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            printf( "Connected to server OK" );
            break;

        default:
            // Silences -Wswitch
            break;
    }
}



bool SocketClient::Connect(SteamNetworkingIPAddr add) {

    printf("trying to connect to server....");


    SteamNetworkingConfigValue_t options;
    options.SetPtr( k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)  OnSteamNetConnectionStatusChanged );

    connection = steamNetworking->ConnectByIPAddress(add, 1, &options );



    return true;
}

int receiveCounter = 0;
void SocketClient::PollIncomingMessages(AUDIO_SAMPLE* _voiceOutputBuffer)
{
    if (connection == k_HSteamNetConnection_Invalid){
        printf("connection is invalid \n");
        return;
    }

    while ( 1 )
    {
        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = steamNetworking->ReceiveMessagesOnConnection(connection, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            break;
        if (numMsgs == -1)
        {
            printf("connection handle is invalid \n");
            break;
        }


        auto* audioData = static_cast<AudioData*>(pIncomingMsg->m_pData);
        if (!audioData) {
            // Handle the case where the cast failed
            pIncomingMsg->Release();
            continue;
        }

        printf("received data from server, size: %u \n", audioData->inputCurrentCounter);

        printf("receive counter is %d \n", ++receiveCounter);
        if (pIncomingMsg->GetSize() == 0) {
            pIncomingMsg->Release();
            continue;
        }

        // Playback
        long delay = 1000000 / 44100;
        const size_t buffer_size = audioData->inputCurrentCounter;
        for (size_t i = 0; i < buffer_size; ++i) {
            if (audioData->Input[i] != 0)
                _voiceOutputBuffer[i] = audioData->Input[i];
            printf("%d," , audioData->Input[i]);
            if (i % 2 == 0)
                std::this_thread::sleep_for( std::chrono::microseconds ( delay ) );
        }
        printf("\n");

        // We don't need this anymore.
        pIncomingMsg->Release();
    }
}

void SocketClient::PollConnectionStateChanges()
{
    steamNetworking->RunCallbacks();
}

SocketClient::SocketClient() {
    // Create client and server sockets
    InitSteamDatagramConnectionSockets();
    steamNetworking = SteamNetworkingSockets();
    SocketClient::Instance = this;
}

SocketClient::~SocketClient() {
    steamNetworking = nullptr;
}


void SocketClient::Send(const void *data, uint32 size) {

    steamNetworking->SendMessageToConnection(connection, data,size,
                                             k_nSteamNetworkingSend_ReliableNoNagle,
                                             nullptr);
}
