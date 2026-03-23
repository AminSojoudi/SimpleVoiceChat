#include "SocketClient.h"
#include "VoiceChatPluginVersion.h"
#include "Utils.h"
#include "../Common/Messages/AudioMessage.h"
#include "../Common/Messages/SetChannelMessage.h"
#include <algorithm>
#include <cstring>
#include <string>

#if defined(_WIN32)
#define EXPORT_API extern "C" __declspec(dllexport)
#else
#define EXPORT_API extern "C" __attribute__((visibility("default")))
#endif

static SocketClient* g_client = nullptr;
static NetworkBuffer g_outputBuffer;
static bool g_initialized = false;

EXPORT_API const char* VC_GetVersionString(void) {
    return VOICECHAT_PLUGIN_VERSION_STRING;
}

EXPORT_API bool VC_Init() {
    if (g_initialized) return true;
    g_client = new SocketClient();
    g_outputBuffer.ResetData();
    g_initialized = true;
    return true;
}

EXPORT_API bool VC_Connect(const char* serverIp, uint16_t port, int64_t channel) {
    if (!g_initialized) return false;
    if (!g_client) return false;
    if (!serverIp || serverIp[0] == '\0') {
        return false;
    }

    SteamNetworkingIPAddr addr;
    addr.Clear();
    std::string address(serverIp);
    address.push_back(':');
    address += std::to_string(port);

    if (!addr.ParseString(address.c_str())) {
        return false;
    }

    bool ok = g_client->Connect(addr);
    if (!ok) return false;

    SetChannel msg;
    msg.channel = channel;
    g_client->Send(&msg, sizeof(msg));

    return true;
}

EXPORT_API void VC_Tick() {
    if (!g_client || !g_initialized) return;
    g_client->PollIncomingMessages(&g_outputBuffer);
    g_client->PollConnectionStateChanges();
}

EXPORT_API bool VC_SendAudio(const int16_t* samples, int sampleCount) {
    if (!g_client || !g_initialized) return false;
    if (sampleCount <= 0 || !samples) return false;

    AudioData packet;
    packet.ResetData();

    constexpr int kMaxSamplesPerPacket = 1024;
    int copy = (std::min)(sampleCount, kMaxSamplesPerPacket);
    for (int i = 0; i < copy; ++i) {
        packet.AddInput((AUDIO_SAMPLE)samples[i]);
    }

    g_client->Send(&packet, sizeof(packet));
    return true;
}

EXPORT_API int VC_RecvAudio(int16_t* outSamples, int maxSamples) {
    if (!g_client || !g_initialized) return 0;
    if (maxSamples <= 0 || !outSamples) return 0;

    int available = (int)g_outputBuffer.Size();
    int toCopy = (std::min)(maxSamples, available);

    for (int i = 0; i < toCopy; ++i) {
        auto maybe = g_outputBuffer.ReadAt(i);
        if (maybe.has_value()) {
            outSamples[i] = (int16_t)maybe.value();
        } else {
            outSamples[i] = 0;
        }
    }

    if (toCopy > 0) {
        g_outputBuffer.RemoveFirstItems(toCopy);
    }
    return toCopy;
}

EXPORT_API void VC_Shutdown() {
    if (!g_initialized) return;
    if (g_client) {
        delete g_client;
        g_client = nullptr;
    }
    g_outputBuffer.ResetData();
    g_initialized = false;
}
