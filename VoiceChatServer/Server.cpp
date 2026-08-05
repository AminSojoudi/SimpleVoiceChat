//
// Created by Amin on 10/15/23.
//
#include "Server.h"

Server* Server::Instance = nullptr;
SteamNetworkingMicroseconds Server::g_logTimeZero;
ISteamNetworkingSockets* Server::steamNetworking;
HSteamNetPollGroup Server::connectionPollGroup;
std::map<HSteamNetConnection, ClientInfo> Server::clients;
uint32 Server::nextClientId = 1;


// Encodes a message into buf. Returns the wire size, or 0 on failure.
template<typename Msg>
static uint32 EncodeMessage(Msg& msg, uint8_t* buf, uint32 capacity) {
    WriteStream stream(buf, capacity);
    if (!msg.Serialize(stream))
        return 0;
    stream.Flush();
    return stream.BytesWritten();
}


void Server::SendMessage(HSteamNetConnection conn, const void* bytes, uint32 size) {
    steamNetworking->SendMessageToConnection(conn, bytes, size,
        k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
    Instance->sentBytesCount += size;
}

void Server::BroadcastToConfigured(const void* bytes, uint32 size, HSteamNetConnection skip) {
    for (auto& [conn, info] : clients) {
        if (!info.hasConfig || conn == skip)
            continue;
        SendMessage(conn, bytes, size);
    }
}

void Server::FillClientEntry(ClientEntry& out, const ClientInfo& info) {
    out.channel = info.channel;
    out.clientId = info.clientId;
    out.muted = info.muted ? 1 : 0;
    snprintf(out.name, sizeof(out.name), "%s", info.name.c_str());
}

void Server::RejectClient(HSteamNetConnection conn, const char* reason) {
    printf("\r rejecting client: %s\n", reason);
    // The reason string rides in the close packet; the client prints it.
    // A locally initiated close never re-enters the disconnect callback,
    // so we erase the entry here.
    steamNetworking->CloseConnection(conn, k_ESteamNetConnectionEnd_App_Generic, reason, false);
    clients.erase(conn);
}


bool Server::StartServer(uint16 port, const std::string& bindAddress, ESteamNetworkingSocketsDebugOutputType logLevel, uint32 audioSampleRate) {
    Instance = this;
    sampleRate = audioSampleRate;

    // Create client and server sockets
    InitSteamDatagramConnectionSockets(logLevel);

    steamNetworking = SteamNetworkingSockets();

    connectionPollGroup = steamNetworking->CreatePollGroup();

    if (steamNetworking == nullptr){
        printf("steam networking is null");
    }

    SteamNetworkingIPAddr addr;
    if (bindAddress.empty()) {
        addr.Clear();
    } else if (!addr.ParseString(bindAddress.c_str())) {
        printf("Failed to parse bind address %s\n", bindAddress.c_str());
        return false;
    }
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
    printf( "Server listening on port %d, sample rate %u Hz\n", port, sampleRate );


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

void Server::InitSteamDatagramConnectionSockets(ESteamNetworkingSocketsDebugOutputType logLevel)
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

    SteamNetworkingUtils()->SetDebugOutputFunction( logLevel, DebugOutput );
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

            // Tell the others before we forget the client. Only clients that
            // reached configured state ever appeared in anyone's client list.
            auto leaver = clients.find( pInfo->m_hConn );
            if ( leaver != clients.end() && leaver->second.hasConfig )
            {
                ClientLeft left;
                left.clientId = leaver->second.clientId;
                uint8_t buf[ClientLeft::MaxWireSize];
                uint32 size = EncodeMessage(left, buf, sizeof(buf));
                if ( size > 0 )
                {
                    printf( "\r client left: id %u (%s)\n", left.clientId, leaver->second.name.c_str() );
                    BroadcastToConfigured( buf, size, pInfo->m_hConn );
                }
            }

            // Forget the client. The entry exists from accept time even if the
            // client never reached the connected state.
            clients.erase( pInfo->m_hConn );

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

            // Track the client from now on. Config fields keep their defaults
            // until the client sends SET_CLIENT_CONFIG.
            char endpoint[ SteamNetworkingIPAddr::k_cchMaxString ];
            pInfo->m_info.m_addrRemote.ToString( endpoint, sizeof(endpoint), true );
            clients[pInfo->m_hConn].endpoint = endpoint;
            clients[pInfo->m_hConn].clientId = nextClientId++;
            printf( "\r assigned client id %u to %s\n", clients[pInfo->m_hConn].clientId, endpoint );

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
    ResetCounters();
    while ( 1)
    {
        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = steamNetworking->ReceiveMessagesOnPollGroup( connectionPollGroup, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            break;
        if ( numMsgs < 0 )
            printf( "Error checking for messages" );
        assert( numMsgs == 1 && pIncomingMsg );

        if (pIncomingMsg->m_pData == NULL || pIncomingMsg->GetSize() == 0){
            printf("received empty message!\n");
            pIncomingMsg->Release();
            continue;
        }

        uint8_t messageType = ((uint8_t*)pIncomingMsg->m_pData)[0];
        receivedBytesCount += pIncomingMsg->GetSize();

        switch (messageType)
        {
            case SET_CLIENT_CONFIG:
            {
                auto it = clients.find(pIncomingMsg->m_conn);
                if (it == clients.end()) {
                    break;
                }

                SetClientConfig config;
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!config.Serialize(stream)) {
                    RejectClient(pIncomingMsg->m_conn, "malformed SetClientConfig");
                    break;
                }
                if (config.protocolVersion != PROTOCOL_VERSION) {
                    char reason[128];
                    snprintf(reason, sizeof(reason), "server protocol %u, client protocol %u",
                        PROTOCOL_VERSION, config.protocolVersion);
                    RejectClient(pIncomingMsg->m_conn, reason);
                    break;
                }

                bool firstConfig = !it->second.hasConfig;
                it->second.channel = config.channel;
                it->second.loopback = config.loopback != 0;
                it->second.muted = config.muted != 0;
                if (config.name[0] == '\0') {
                    char fallback[32];
                    snprintf(fallback, sizeof(fallback), "client-%u", it->second.clientId);
                    it->second.name = fallback;
                } else {
                    it->second.name = config.name;
                }
                it->second.hasConfig = true;
                printf("\r client config received from %s: id %u, name %s, channel %lld, loopback %d, muted %d\n",
                    it->second.endpoint.c_str(), it->second.clientId, it->second.name.c_str(),
                    (long long)config.channel, (int)config.loopback, (int)config.muted);

                if (firstConfig) {
                    // Send the full client list to the new client.
                    static ClientList list; // static: too big for the stack next to its buffer
                    list.clientCount = 0;
                    for (auto& [conn, info] : clients) {
                        if (!info.hasConfig)
                            continue;
                        if (list.clientCount >= ClientList::MaxClients) {
                            printf("\r warning: more than %u configured clients, client list truncated\n",
                                ClientList::MaxClients);
                            break;
                        }
                        FillClientEntry(list.entries[list.clientCount++], info);
                    }
                    uint8_t listBuf[ClientList::MaxWireSize];
                    uint32 listSize = EncodeMessage(list, listBuf, sizeof(listBuf));
                    if (listSize > 0)
                        SendMessage(pIncomingMsg->m_conn, listBuf, listSize);

                    // Tell everyone else about the newcomer.
                    ClientJoined joined;
                    FillClientEntry(joined.entry, it->second);
                    uint8_t buf[ClientJoined::MaxWireSize];
                    uint32 size = EncodeMessage(joined, buf, sizeof(buf));
                    if (size > 0)
                        BroadcastToConfigured(buf, size, pIncomingMsg->m_conn);
                } else {
                    // Config update: broadcast to everyone including the sender.
                    ClientConfigChanged changed;
                    FillClientEntry(changed.entry, it->second);
                    uint8_t buf[ClientConfigChanged::MaxWireSize];
                    uint32 size = EncodeMessage(changed, buf, sizeof(buf));
                    if (size > 0)
                        BroadcastToConfigured(buf, size, k_HSteamNetConnection_Invalid);
                }
                break;
            }
            case AUDIO:
            {
                auto sender = clients.find(pIncomingMsg->m_conn);
                if (sender == clients.end() || !sender->second.hasConfig) {
                    break;
                }

                // Validate without a full decode; this is the hot path.
                // Wire layout: type u8, senderId u32 at bytes 1-4, count u32 at bytes 5-8, samples.
                const uint32 size = pIncomingMsg->GetSize();
                uint8_t* bytes = (uint8_t*)pIncomingMsg->m_pData;
                if (size < 9) {
                    printf("\r dropped audio message that is too short: %u bytes\n", size);
                    break;
                }
                uint32 count;
                memcpy(&count, bytes + 5, 4);
                if (count > AudioData::Capacity || 9 + 2 * count != size) {
                    printf("\r dropped audio message claiming %u samples in %u bytes\n", count, size);
                    break;
                }

                // Stamp the sender id at its fixed wire offset. Clients cannot
                // spoof it: whatever they sent gets overwritten here. The same
                // buffer then goes to every recipient.
                uint32 senderId = sender->second.clientId;
                memcpy(bytes + 1, &senderId, 4);

                for (auto& [conn, info] : clients) {
                    if (!info.hasConfig || info.channel != sender->second.channel) {
                        continue;
                    }
                    // The sender only hears themselves if they asked for loopback.
                    if (conn == pIncomingMsg->m_conn && !sender->second.loopback) {
                        continue;
                    }
                    SendMessage(conn, bytes, size);
                }
                break;
            }
            case GET_SERVER_INFO:
            {
                ServerInfoRequest request;
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!request.Serialize(stream)) {
                    RejectClient(pIncomingMsg->m_conn, "malformed ServerInfoRequest - client too old?");
                    break;
                }
                if (request.protocolVersion != PROTOCOL_VERSION) {
                    char reason[128];
                    snprintf(reason, sizeof(reason), "server protocol %u, client protocol %u",
                        PROTOCOL_VERSION, request.protocolVersion);
                    RejectClient(pIncomingMsg->m_conn, reason);
                    break;
                }

                ServerInfo serverInfo;
                serverInfo.sampleRate = sampleRate;
                uint8_t buf[ServerInfo::MaxWireSize];
                uint32 size = EncodeMessage(serverInfo, buf, sizeof(buf));
                if (size > 0) {
                    SendMessage(pIncomingMsg->m_conn, buf, size);
                    printf("\r server info requested, sent sample rate %u", sampleRate);
                }
                break;
            }
            default:
            {
                auto it = clients.find(pIncomingMsg->m_conn);
                if (it == clients.end() || !it->second.hasConfig) {
                    // Likely a pre-phase-1 client. Give it a clear reason
                    // instead of a 30 second hang.
                    char reason[96];
                    snprintf(reason, sizeof(reason), "unrecognized message type %u; server protocol %u",
                        messageType, PROTOCOL_VERSION);
                    RejectClient(pIncomingMsg->m_conn, reason);
                } else {
                    printf("\r ignoring unrecognized message type %u from a configured client\n", messageType);
                }
                break;
            }
        }

        // We don't need this anymore.
        pIncomingMsg->Release();


    }
}

void Server::PollConnectionStateChanges() {
    steamNetworking->RunCallbacks();
}

uint16 Server::GetSentBytes()
{
    return sentBytesCount * 8 / 1024;
}

uint16 Server::GetRecievedBytes()
{
    return receivedBytesCount * 8 / 1024;
}

bool Server::ResetCounters()
{
    sentBytesCount = 0;
    receivedBytesCount = 0;
    return true;
}
