#pragma once

#include <string>
#include <optional>
#include <steam/isteamnetworkingutils.h>

// Polling slower than once a second is useless for voice, and capping it here keeps
// the audio buffer size we derive from it small enough to compute safely.
const unsigned int MinSyncIntervalMs = 1;
const unsigned int MaxSyncIntervalMs = 1000;

// Sample rates we accept from a server. Anything outside this is a broken server.
const unsigned int MinSampleRate = 8000;
const unsigned int MaxSampleRate = 192000;

struct ClientConfig {
    std::string serverAddress = "127.0.0.1";
    uint16_t port = 27020;
    int64_t channel = 0;
    std::string name; // shown to other clients; empty means the server picks client-<id>
    bool loopback = false; // hear your own voice echoed back from the server
    ESteamNetworkingSocketsDebugOutputType logLevel = k_ESteamNetworkingSocketsDebugOutputType_Msg;
    unsigned int syncIntervalMs = 5; // how often we poll the network, also sets the audio buffer size
};

class ConfigParser {
public:
    // Parses argv into `out`. Returns an exit code if the caller should stop and exit right away
    // (0 after printing --help, 1 after printing a parse error). Returns nullopt on success.
    static std::optional<int> Parse(int argc, const char *argv[], ClientConfig &out);

private:
    static bool TryParseLogLevel(const std::string &value, ESteamNetworkingSocketsDebugOutputType &out);
};
