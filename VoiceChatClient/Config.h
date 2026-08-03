#pragma once

#include <string>
#include <optional>
#include <steam/isteamnetworkingutils.h>

struct ClientConfig {
    std::string serverAddress = "127.0.0.1";
    uint16_t port = 27020;
    int64_t channel = 0;
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
