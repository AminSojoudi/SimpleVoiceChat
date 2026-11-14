#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "SampleRing.h"

class RtAudio;
struct AudioData;
struct ClientEntry;
struct ServerInfo;
struct SteamNetConnectionStatusChangedCallback_t;

enum class LogLevel { Error, Warning, Info, Debug };

// Options you can change during a session with UpdateOptions.
struct VoiceChatOptions {
    int64_t channel = 0;
    bool muted = false;
    bool loopback = false;  // hear your own voice back from the server
};

struct VoiceChatConfig {
    std::string address = "127.0.0.1";
    uint16_t port = 27020;
    std::string name;                 // empty: the server picks client-<id>
    unsigned int syncIntervalMs = 5;  // audio buffer length; call Update at least this often
    LogLevel logLevel = LogLevel::Info;
    VoiceChatOptions options;         // options at connect time
};

// A client on the server, on any channel. Includes me (id == SelfId()).
struct Peer {
    uint32_t id = 0;
    std::string name;
    int64_t channel = 0;
    bool muted = false;
    bool talking = false;
};

enum class EventType {
    CONNECTED,        // handshake done, audio running
    DISCONNECTED,     // text = reason (server closed, rejected, timeout, audio failure)
    PEER_LIST,        // full list arrived; read Peers()
    PEER_JOINED,      // peer = the new client
    PEER_LEFT,        // peer = the client that left (already removed from Peers())
    PEER_CHANGED,     // peer = new state (channel, muted, name)
    TALKING_CHANGED,  // peer.talking changed; peer.id == SelfId() means my mic
    LOG,              // logLevel + text
};

struct Event {
    EventType type;
    Peer peer;
    LogLevel logLevel = LogLevel::Info;
    std::string text;
};

enum class SessionState { DISCONNECTED, CONNECTING, CONNECTED };

// One voice chat connection. Only one can exist per process, because GNS
// callbacks carry no user pointer. Call every method from one thread.
// Events fire on that thread inside Update(). A failed Connect also fires
// LOG events before it returns. Don't call Connect or Disconnect from the
// event callback.
class VoiceChatSession {
public:
    explicit VoiceChatSession(std::function<void(const Event&)> onEvent);
    ~VoiceChatSession();                        // Disconnect, then GameNetworkingSockets_Kill

    bool Connect(const VoiceChatConfig& config); // false = failed at once; a LOG event says why
    void Disconnect();                           // stop audio, close the connection; no event
    void Update();                               // pump network + queued logs, fire events
    void UpdateOptions(const VoiceChatOptions& options);

    SessionState State() const { return state; }
    const VoiceChatOptions& Options() const { return config.options; }
    const std::vector<Peer>& Peers() const { return peers; }  // all channels; filter yourself
    uint32_t SelfId() const { return selfId; }               // 0 until connected
    float MicLevel() const { return micLevel; }              // 0..1, peak of the last mic buffer

private:
    // ponytail: fixed slot count; raise it if channels get bigger than this
    static constexpr int MaxSenders = 8;

    // One sender's playback queue.
    struct PlaybackSlot {
        std::atomic<uint32_t> senderId{0};  // 0 = free. Written by the Update thread.
        std::atomic<bool> clear{false};     // Update thread sets it, audio thread empties the ring and resets it
        SampleRing ring;                    // producer: Update thread, consumer: audio thread
    };

    // What the Update thread knows about one sender's audio.
    struct SenderAudio {
        uint32_t lastSequence = 0;
        std::chrono::steady_clock::time_point lastHeard;
        bool talking = false;
    };

    // GNS and RtAudio callbacks, defined in the .cpp
    friend struct SessionCallbacks;

    // Update thread
    void HandleStatus(SteamNetConnectionStatusChangedCallback_t* info);
    void Drop(const std::string& reason);
    void ReceiveMessages();
    void HandleMessage(const uint8_t* bytes, uint32_t size);
    void HandleServerInfo(const ServerInfo& info);
    void HandleAudio(const AudioData& packet);
    Peer ToPeer(const ClientEntry& entry) const;
    void SendConfig();
    bool StartAudio(unsigned int sampleRate);
    void FreeSlot(uint32_t senderId);
    void SetTalking(uint32_t id, bool talking);
    void UpdateTalking();
    void Fire(const Event& event);
    void Log(LogLevel level, const std::string& text);
    void DrainLogs();

    // Any thread
    void QueueLog(LogLevel level, const std::string& text);

    // Audio thread
    void SendAudio();

    std::function<void(const Event&)> onEvent;
    VoiceChatConfig config;  // config.options holds the current options
    SessionState state = SessionState::DISCONNECTED;
    bool initialized = false;
    std::vector<Peer> peers;
    uint32_t selfId = 0;
    std::chrono::steady_clock::time_point deadline;  // handshake timeout
    std::map<uint32_t, SenderAudio> senders;
    bool selfTalking = false;
    size_t droppedSamples = 0;   // ring full, reported once per Update
    size_t droppedPackets = 0;   // no free slot, reported once per Update
    uint32_t reportedOverflows = 0;

    std::mutex logMutex;
    std::vector<std::pair<LogLevel, std::string>> queuedLogs;

    // Shared with the audio thread
    std::atomic<uint32_t> connection{0};  // HSteamNetConnection, 0 = none
    std::atomic<bool> muted{false};
    std::atomic<float> micLevel{0};
    std::atomic<bool> sending{false};  // mic is loud or was within TalkingHold, and not muted
    std::atomic<uint32_t> audioOverflows{0};
    std::array<PlaybackSlot, MaxSenders> slots;

    // Audio thread only, sized before the stream starts
    std::unique_ptr<RtAudio> audio;
    std::unique_ptr<AudioData> capture;
    uint32_t nextSequence = 0;
    uint32_t holdBuffers = 0;   // TalkingHold in audio buffers
    uint32_t quietBuffers = 0;  // buffers since the mic was last above TalkingLevel, up to holdBuffers
    std::vector<uint16_t> scratch;
    std::vector<int32_t> mix;
};
