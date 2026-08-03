#include "Config.h"

#include <cxxopts.hpp>
#include <iostream>
#include <algorithm>
#include <cctype>

bool ConfigParser::TryParseLogLevel(const std::string &value, ESteamNetworkingSocketsDebugOutputType &out) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

    if (lower == "none") { out = k_ESteamNetworkingSocketsDebugOutputType_None; return true; }
    if (lower == "error") { out = k_ESteamNetworkingSocketsDebugOutputType_Error; return true; }
    if (lower == "warning") { out = k_ESteamNetworkingSocketsDebugOutputType_Warning; return true; }
    if (lower == "info") { out = k_ESteamNetworkingSocketsDebugOutputType_Msg; return true; }
    if (lower == "verbose") { out = k_ESteamNetworkingSocketsDebugOutputType_Verbose; return true; }
    if (lower == "debug") { out = k_ESteamNetworkingSocketsDebugOutputType_Debug; return true; }
    if (lower == "everything") { out = k_ESteamNetworkingSocketsDebugOutputType_Everything; return true; }

    return false;
}

std::optional<int> ConfigParser::Parse(int argc, const char *argv[], ClientConfig &out) {
    cxxopts::Options options("VoiceChatClient", "Simple UDP voice chat client");
    options.add_options()
        ("a,address", "Server address", cxxopts::value<std::string>()->default_value("127.0.0.1"))
        ("p,port", "Server UDP port", cxxopts::value<uint16_t>()->default_value("27020"))
        ("c,channel", "Voice channel", cxxopts::value<int64_t>()->default_value("0"))
        ("l,log-level", "Log level: none|error|warning|info|verbose|debug|everything", cxxopts::value<std::string>()->default_value("info"))
        ("s,sync-interval", "How often to poll the network, in milliseconds (1 to 1000)", cxxopts::value<unsigned int>()->default_value("5"))
        ("h,help", "Print usage");

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        std::string logLevelStr = result["log-level"].as<std::string>();
        ESteamNetworkingSocketsDebugOutputType logLevel;
        if (!TryParseLogLevel(logLevelStr, logLevel)) {
            std::cerr << "Invalid log level '" << logLevelStr << "'. Valid values: none, error, warning, info, verbose, debug, everything" << std::endl;
            return 1;
        }

        // Keep the interval in a range that makes sense for voice. This also keeps the
        // audio buffer size we derive from it well inside what we can compute safely.
        unsigned int syncIntervalMs = result["sync-interval"].as<unsigned int>();
        if (syncIntervalMs < MinSyncIntervalMs || syncIntervalMs > MaxSyncIntervalMs) {
            std::cerr << "Invalid sync interval " << syncIntervalMs << ": must be between "
                      << MinSyncIntervalMs << " and " << MaxSyncIntervalMs << " ms" << std::endl;
            return 1;
        }

        out.serverAddress = result["address"].as<std::string>();
        out.port = result["port"].as<uint16_t>();
        out.channel = result["channel"].as<int64_t>();
        out.logLevel = logLevel;
        out.syncIntervalMs = syncIntervalMs;

        return std::nullopt;
    } catch (const cxxopts::exceptions::parsing &e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }
}
