//
// Created by Amin on 10/17/23.
//

#include "SocketClient.h"

SteamNetworkingMicroseconds SocketClient::g_logTimeZero;
HSteamNetConnection SocketClient::connection;
ISteamNetworkingSockets* SocketClient::steamNetworking;
bool SocketClient::isConnected = false;
ServerInfo SocketClient::serverInfo;
bool SocketClient::hasServerInfo = false;


void SocketClient::InitSteamDatagramConnectionSockets(ESteamNetworkingSocketsDebugOutputType logLevel) {
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
            // Let the wait loops in main see the drop, e.g. a server rejection.
            isConnected = false;
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
            // We will get this callback when we start connecting.
            // We can ignore this.
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            printf( "Connected to server OK" );
            isConnected = true;
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

    if ( connection == k_HSteamNetConnection_Invalid )
    {
        printf( "Failed to create connection" );
        return false;
    }

    return true;
}

void SocketClient::PollIncomingMessages(NetworkBuffer* _voiceAudioBuffer)
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


        if (pIncomingMsg->m_pData == nullptr || pIncomingMsg->GetSize() == 0) {
            pIncomingMsg->Release();
            continue;
        }

        uint8_t messageType = ((uint8_t*)pIncomingMsg->m_pData)[0];

        switch (messageType)
        {
            case AUDIO:
            {
                // Range checks live in serialize_sample_array; a failed decode drops the message.
                static AudioData audioData; // static: too big for the stack
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!audioData.Serialize(stream)) {
                    printf("dropping malformed audio message of %u bytes \n", pIncomingMsg->GetSize());
                    break;
                }

                // Playback. No senderId printing here: latency. Phase 3 consumes it.
                for (uint32 i = 0; i < audioData.sampleCount; ++i) {
                    _voiceAudioBuffer->AddInput(audioData.Input[i]);
                }
                break;
            }
            case SERVER_INFO:
            {
                ServerInfo info;
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!info.Serialize(stream)) {
                    printf("dropping malformed server info message \n");
                    break;
                }
                serverInfo = info;
                hasServerInfo = true;
                printf("received server info, sample rate %u, protocol %u \n",
                    serverInfo.sampleRate, serverInfo.protocolVersion);
                break;
            }
            case CLIENT_LIST:
            {
                static ClientList list; // static: too big for the stack
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!list.Serialize(stream)) {
                    printf("dropping malformed client list \n");
                    break;
                }
                printf("client list: %u clients online \n", list.clientCount);
                for (uint32 i = 0; i < list.clientCount; ++i) {
                    const ClientEntry& e = list.entries[i];
                    printf("  id %u, name %s, channel %lld, muted %d \n",
                        e.clientId, e.name, (long long)e.channel, (int)e.muted);
                }
                fflush(stdout);
                break;
            }
            case CLIENT_JOINED:
            {
                ClientJoined joined;
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!joined.Serialize(stream)) {
                    printf("dropping malformed client joined event \n");
                    break;
                }
                printf("client joined: %s (id %u, channel %lld, muted %d) \n",
                    joined.entry.name, joined.entry.clientId,
                    (long long)joined.entry.channel, (int)joined.entry.muted);
                fflush(stdout);
                break;
            }
            case CLIENT_CONFIG_CHANGED:
            {
                ClientConfigChanged changed;
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!changed.Serialize(stream)) {
                    printf("dropping malformed client config changed event \n");
                    break;
                }
                printf("client config changed: %s (id %u, channel %lld, muted %d) \n",
                    changed.entry.name, changed.entry.clientId,
                    (long long)changed.entry.channel, (int)changed.entry.muted);
                fflush(stdout);
                break;
            }
            case CLIENT_LEFT:
            {
                ClientLeft left;
                ReadStream stream(pIncomingMsg->m_pData, pIncomingMsg->GetSize());
                if (!left.Serialize(stream)) {
                    printf("dropping malformed client left event \n");
                    break;
                }
                printf("client left: id %u \n", left.clientId);
                fflush(stdout);
                break;
            }
            default:
                break;
        }

        // We don't need this anymore.
        pIncomingMsg->Release();
    }
}

void SocketClient::PollConnectionStateChanges()
{
    steamNetworking->RunCallbacks();
}

SocketClient::SocketClient(ESteamNetworkingSocketsDebugOutputType logLevel) {
    // Create client and server sockets
    InitSteamDatagramConnectionSockets(logLevel);
    steamNetworking = SteamNetworkingSockets();
}

SocketClient::~SocketClient() {
    // Close cleanly so the server broadcasts our leave right away instead of
    // waiting for a timeout.
    if (connection != k_HSteamNetConnection_Invalid) {
        steamNetworking->CloseConnection(connection, 0, "client quit", true);
        connection = k_HSteamNetConnection_Invalid;
    }
    isConnected = false;
    steamNetworking = nullptr;
}


void SocketClient::Send(const void *data, uint32 size) {

    steamNetworking->SendMessageToConnection(connection, data,size,
                                             k_nSteamNetworkingSend_ReliableNoNagle,
                                             nullptr);
}

bool SocketClient::IsConnected() {
    return isConnected;
}

bool SocketClient::HasServerInfo() {
    return hasServerInfo;
}

const ServerInfo& SocketClient::GetServerInfo() {
    return serverInfo;
}

void SocketClient::RequestServerInfo() {
    ServerInfoRequest message;
    uint8_t buf[ServerInfoRequest::MaxWireSize];
    WriteStream stream(buf, sizeof(buf));
    if (!message.Serialize(stream)) {
        printf("failed to encode server info request \n");
        return;
    }
    stream.Flush();
    Send(buf, stream.BytesWritten());
}
