//
// Created by Amin on 10/15/23.
//

#ifndef VALVEGAMESERVERTEST_SERVER_H
#define VALVEGAMESERVERTEST_SERVER_H

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <stdio.h>
#include <cassert>
#include <thread>
#include "queue"

#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif


class Server {
private:
    HSteamListenSocket socket;
    static ISteamNetworkingSockets* steamNetworking;
    static SteamNetworkingMicroseconds g_logTimeZero;
    static HSteamNetPollGroup connectionPollGroup;

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


#endif //VALVEGAMESERVERTEST_SERVER_H
