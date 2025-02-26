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
#include <csignal>
#else
#include <arpa/inet.h>
#endif


#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <stdio.h>
#include <cassert>
#include <thread>
#include "queue"

#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif


#include "../Common/Messages/MessageTypes.h"
#include "../Common/Messages/AudioMessage.h"
#include "../Common/Messages/SetChannelMessage.h"
#include <map>
#include <set>


class Server {
private:
    HSteamListenSocket socket;
    static ISteamNetworkingSockets* steamNetworking;
    static SteamNetworkingMicroseconds g_logTimeZero;
    static HSteamNetPollGroup connectionPollGroup;
    static std::map<int64, std::set<HSteamNetConnection>> channelToConnnectionsMap;

    static void InitSteamDatagramConnectionSockets();
    static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg );
    static void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo );

public:
    static Server* Instance;
    bool  StartServer(uint16 port);
    void PollIncomingMessages();
    void PollConnectionStateChanges();
    ~Server();
};


#endif //VOICECHATSERVER_SERVER_H
