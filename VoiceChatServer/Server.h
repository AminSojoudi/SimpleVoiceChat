//
// Created by Amin on 10/15/23.
//

#ifndef VOICECHATSERVER_SERVER_H
#define VOICECHATSERVER_SERVER_H

#define PLATFORM_WINDOWS  1
#define PLATFORM_MAC      2
#define PLATFORM_UNIX     3

#if defined(_WIN32)
#define PLATFORM PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define PLATFORM PLATFORM_MAC
#else
#define PLATFORM PLATFORM_UNIX
#endif


#if PLATFORM == PLATFORM_WINDOWS
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <string>
#else
#include <arpa/inet.h>
#endif


#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <stdio.h>
#include <cassert>
#include <thread>
#include "queue"
#include <csignal>

#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif


#include "../Common/Messages/MessageTypes.h"
#include "../Common/Messages/AudioMessage.h"
#include "../Common/Messages/ClientConfigMessage.h"
#include "../Common/Messages/ServerInfoRequestMessage.h"
#include "../Common/Messages/ServerInfoMessage.h"
#include "../Common/Messages/ClientListMessages.h"
#include "../Common/Serialization/Streams.h"
#include <map>
#include <string>

// Windows headers define SendMessage as a macro. We use the name for our own
// send helper, and nothing here calls the Win32 function.
#ifdef SendMessage
#undef SendMessage
#endif


// Everything the server knows about one connected client.
struct ClientInfo
{
    // Set by the client via SET_CLIENT_CONFIG. Defaults until the first config
    // arrives. hasConfig true also means the protocol version checked out.
    int64 channel = 0;
    bool loopback = false;
    bool muted = false;
    bool hasConfig = false; // no audio is relayed to or from a client until this is true

    // Server-derived.
    uint32 clientId = 0; // assigned at accept time, monotonic from 1, 0 = unassigned
    std::string name;    // empty until config; server substitutes client-<id>
    std::string endpoint; // remote IP:port, captured when the connection is accepted
};


class Server {
private:
    HSteamListenSocket socket;
    uint16 sentBytesCount;
    uint16 receivedBytesCount;
    uint32 sampleRate;
    static ISteamNetworkingSockets* steamNetworking;
    static SteamNetworkingMicroseconds g_logTimeZero;
    static HSteamNetPollGroup connectionPollGroup;
    static std::map<HSteamNetConnection, ClientInfo> clients;
    static uint32 nextClientId;

    static void InitSteamDatagramConnectionSockets(ESteamNetworkingSocketsDebugOutputType logLevel);
    static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg );
    static void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo );

    // Sends raw bytes over one connection and counts them. All send sites go through this.
    static void SendMessage(HSteamNetConnection conn, const void* bytes, uint32 size);
    // Sends to every client that has config, except `skip` (pass k_HSteamNetConnection_Invalid for no skip).
    static void BroadcastToConfigured(const void* bytes, uint32 size, HSteamNetConnection skip);
    static void FillClientEntry(ClientEntry& out, const ClientInfo& info);
    // Logs, closes the connection with the reason string, and drops the client
    // entry. Callers must drop their iterators after calling this.
    static void RejectClient(HSteamNetConnection conn, const char* reason);

public:
    static Server* Instance;
    bool  StartServer(uint16 port, const std::string& bindAddress, ESteamNetworkingSocketsDebugOutputType logLevel, uint32 audioSampleRate);
    void PollIncomingMessages();
    void PollConnectionStateChanges();
    uint16 GetSentBytes();
    uint16 GetRecievedBytes();
    bool ResetCounters();
    ~Server();
};


#endif //VOICECHATSERVER_SERVER_H
