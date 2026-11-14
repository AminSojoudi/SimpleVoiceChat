#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "Config.h"
#include "../Common/Serialization/AudioStats.h"

// Set by Ctrl+C so the main loop can exit and clean up.
static std::atomic<bool> g_quit{false};

static void OnSignal(int) {
    g_quit = true;
}

// Lines typed on stdin. The input thread adds them, the main loop takes them.
static std::mutex g_inputMutex;
static std::deque<std::string> g_input;

static void ReadInput() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::lock_guard<std::mutex> lock(g_inputMutex);
        g_input.push_back(line);
    }
}

static std::string Describe(const Peer& peer) {
    return std::to_string(peer.id) + " " + peer.name + " (channel " + std::to_string(peer.channel) +
           (peer.muted ? ", muted)" : ")");
}

int main(int argc, const char *argv[]) {
    VoiceChatConfig config;
    if (auto exitCode = ConfigParser::Parse(argc, argv, config)) {
        return *exitCode;
    }

    bool dropped = false;
    std::unique_ptr<VoiceChatSession> session;  // a pointer, so the callback below can use it
    session = std::make_unique<VoiceChatSession>([&](const Event& event) {
        switch (event.type) {
        case EventType::CONNECTED: printf("connected as %u\n", session->SelfId()); break;
        case EventType::DISCONNECTED: printf("disconnected: %s\n", event.text.c_str()); dropped = true; break;
        case EventType::PEER_LIST: {
            std::string list;
            for (const Peer& peer : session->Peers()) list += " " + Describe(peer);
            printf("peers:%s\n", list.c_str());
            break;
        }
        case EventType::PEER_JOINED: printf("joined %s\n", Describe(event.peer).c_str()); break;
        case EventType::PEER_LEFT: printf("left %s\n", Describe(event.peer).c_str()); break;
        case EventType::PEER_CHANGED: printf("changed %s\n", Describe(event.peer).c_str()); break;
        case EventType::TALKING_CHANGED:
            printf("%s %s talking\n", Describe(event.peer).c_str(), event.peer.talking ? "started" : "stopped");
            break;
        case EventType::LOG: printf("%s\n", event.text.c_str()); break;
        }
        fflush(stdout);  // stdout may be a pipe or a file
    });

    if (!session->Connect(config)) {
        return 1;
    }
    std::signal(SIGINT, OnSignal);
    std::thread(ReadInput).detach();

    bool quit = false;
    while (!quit && !g_quit && session->State() != SessionState::DISCONNECTED) {
        std::deque<std::string> lines;
        {
            std::lock_guard<std::mutex> lock(g_inputMutex);
            lines.swap(g_input);
        }
        VoiceChatOptions options = session->Options();
        long long channel;
        for (const std::string& line : lines) {
            if (line == "m") options.muted = !options.muted;
            else if (line == "l") options.loopback = !options.loopback;
            else if (sscanf(line.c_str(), "c %lld", &channel) == 1) options.channel = channel;
            else if (line == "q") quit = true;
        }
        // Skip the resend when only q or unknown lines came in.
        const VoiceChatOptions& old = session->Options();
        if (options.muted != old.muted || options.loopback != old.loopback || options.channel != old.channel)
            session->UpdateOptions(options);

        session->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(config.syncIntervalMs));
    }

    session->Disconnect();
    VOICECHAT_AUDIO_STATS_PRINT();
    return dropped ? 1 : 0;
}
