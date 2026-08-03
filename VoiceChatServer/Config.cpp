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

std::optional<int> ConfigParser::Parse(int argc, const char *argv[], ServerConfig &out) {
    cxxopts::Options options("VoiceChatServer", "Simple UDP voice chat server");
    options.add_options()
        ("p,port", "UDP port to listen on", cxxopts::value<uint16_t>()->default_value("27020"))
        ("b,bind", "Bind address (defaults to any interface)", cxxopts::value<std::string>()->default_value(""))
        ("l,log-level", "Log level: none|error|warning|info|verbose|debug|everything", cxxopts::value<std::string>()->default_value("info"))
        ("s,sample-rate", "Audio sample rate in Hz that all clients use", cxxopts::value<unsigned int>()->default_value("44100"))
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

        unsigned int sampleRate = result["sample-rate"].as<unsigned int>();
        if (sampleRate == 0) {
            std::cerr << "Invalid sample rate: must be greater than 0" << std::endl;
            return 1;
        }

        out.port = result["port"].as<uint16_t>();
        out.bindAddress = result["bind"].as<std::string>();
        out.logLevel = logLevel;
        out.sampleRate = sampleRate;

        return std::nullopt;
    } catch (const cxxopts::exceptions::parsing &e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }
}
