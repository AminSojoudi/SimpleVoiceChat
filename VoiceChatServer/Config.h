#pragma once

#include <string>
#include <optional>
#include <steam/isteamnetworkingutils.h>

struct ServerConfig {
    uint16_t port = 27020;
    std::string bindAddress; // empty means bind to any interface
    ESteamNetworkingSocketsDebugOutputType logLevel = k_ESteamNetworkingSocketsDebugOutputType_Msg;
    unsigned int sampleRate = 44100; // clients ask for this and use it for their audio streams
};

class ConfigParser {
public:
    // Parses argv into `out`. Returns an exit code if the caller should stop and exit right away
    // (0 after printing --help, 1 after printing a parse error). Returns nullopt on success.
    static std::optional<int> Parse(int argc, const char *argv[], ServerConfig &out);

private:
    static bool TryParseLogLevel(const std::string &value, ESteamNetworkingSocketsDebugOutputType &out);
};
