#pragma once

#include <optional>
#include "VoiceChatSession.h"

// Polling slower than once a second is useless for voice, and capping it here keeps
// the audio buffer size we derive from it small enough to compute safely.
const unsigned int MinSyncIntervalMs = 1;
const unsigned int MaxSyncIntervalMs = 1000;

class ConfigParser {
public:
    // Parses argv into `out`. Returns an exit code if the caller should stop and exit right away
    // (0 after printing --help, 1 after printing a parse error). Returns nullopt on success.
    static std::optional<int> Parse(int argc, const char *argv[], VoiceChatConfig &out);

private:
    static bool TryParseLogLevel(const std::string &value, LogLevel &out);
};
