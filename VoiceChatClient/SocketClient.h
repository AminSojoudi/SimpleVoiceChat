//
// Created by Amin on 10/17/23.
//

#ifndef VOICECHATCLIENT_SOCKETCLIENT_H
#define VOICECHATCLIENT_SOCKETCLIENT_H

#include "Message.h"



#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

class SocketClient {
private:
    static HSteamNetConnection connection;
    static ISteamNetworkingSockets* steamNetworking;
    static SteamNetworkingMicroseconds g_logTimeZero;
    static bool isConnected;

    static void InitSteamDatagramConnectionSockets();
    static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg );
    static void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo );

public:
    bool IsConnected();
    bool Connect(SteamNetworkingIPAddr add);
    void PollIncomingMessages(AUDIO_SAMPLE* _voiceOutputBuffer);
    void PollConnectionStateChanges();
    void Send(const void* data, uint32 size);
    SocketClient();
    ~SocketClient();
};


#endif //VOICECHATCLIENT_SOCKETCLIENT_H
