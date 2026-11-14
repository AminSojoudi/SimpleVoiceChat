#include "VoiceChatSession.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <rtaudio/RtAudio.h>
#include "../Common/Messages/AudioMessage.h"
#include "../Common/Messages/ClientConfigMessage.h"
#include "../Common/Messages/ClientListMessages.h"
#include "../Common/Messages/ServerInfoMessage.h"
#include "../Common/Messages/ServerInfoRequestMessage.h"
#include "../Common/Serialization/AudioStats.h"
#include "../Common/Serialization/Streams.h"

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto HandshakeTimeout = 30s;
constexpr auto TalkingHold = 200ms;
constexpr auto SenderTimeout = 2s;  // a sender silent this long gives up its playback slot

// Mic peak above this counts as talking. Calibration knob: about -34 dBFS.
constexpr float TalkingLevel = 0.02f;

// ponytail: silence is sized from this packet's count; a jitter buffer would time it properly
constexpr uint32_t MaxGapPackets = 2;

// Sample rates we accept from a server. Anything outside this is a broken server.
constexpr unsigned int MinSampleRate = 8000;
constexpr unsigned int MaxSampleRate = 192000;

// Playback ring size in audio buffers. Arrival and playback drift against
// each other; a few buffers of headroom absorb the jitter. More also means
// more worst-case latency, so keep it small.
constexpr size_t PlaybackSlackBuffers = 4;
// Floor for the ring size: a tenth of a second. Short buffers need it because
// OS sleep jitter (~16 ms on Windows) spans many of them.
constexpr size_t PlaybackSlackMinFractionOfSecond = 10;

// GNS callbacks carry no user pointer, so they find the session here.
static VoiceChatSession* g_session = nullptr;

static std::string Format(const char* format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}

// Encodes a message and sends it. SendToConnection picks lane and reliability.
template<typename T>
static void Send(HSteamNetConnection conn, T& message) {
    uint8_t buf[T::MaxWireSize];
    WriteStream stream(buf, sizeof(buf));
    if (!message.Serialize(stream)) return;  // the buffer fits MaxWireSize, so this can't fail
    stream.Flush();
    SendToConnection(SteamNetworkingSockets(), conn, buf, stream.BytesWritten());
}

template<typename T>
static bool Decode(T& message, const uint8_t* bytes, uint32_t size) {
    ReadStream stream(bytes, size);
    return message.Serialize(stream);
}

// Takes talking from what this session already knows. My mic can start
// talking before the peer list arrives.
Peer VoiceChatSession::ToPeer(const ClientEntry& entry) const {
    Peer peer;
    peer.id = entry.clientId;
    peer.name = entry.name;
    peer.channel = entry.channel;
    peer.muted = entry.muted != 0;
    auto it = senders.find(peer.id);
    peer.talking = peer.id == selfId ? selfTalking : it != senders.end() && it->second.talking;
    return peer;
}

struct SessionCallbacks {
    // Runs inside RunCallbacks, on the Update thread.
    static void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t* info) {
        g_session->HandleStatus(info);
    }

    // Runs on any GNS thread.
    static void OnDebugOutput(ESteamNetworkingSocketsDebugOutputType type, const char* text) {
        LogLevel level = type <= k_ESteamNetworkingSocketsDebugOutputType_Error ? LogLevel::Error
                       : type <= k_ESteamNetworkingSocketsDebugOutputType_Warning ? LogLevel::Warning
                       : type <= k_ESteamNetworkingSocketsDebugOutputType_Msg ? LogLevel::Info
                       : LogLevel::Debug;
        if (g_session) g_session->QueueLog(level, text);
    }

    // Realtime audio thread. Logs nothing itself, but sending can block for a
    // moment: GNS SendMessages takes an internal lock and allocates, and GNS
    // may queue a debug log under logMutex.
    // ponytail: sends from the audio thread; if overflows show up, move sends to Update through a second ring
    static int OnAudio(void* output, void* input, unsigned int frames, double, RtAudioStreamStatus status, void* userData) {
        auto* s = static_cast<VoiceChatSession*>(userData);
        if (status) s->audioOverflows++;

        // Mic: peak level, then the silence gate. Send only while the mic is
        // loud or was loud within TalkingHold, so a quiet room sends nothing.
        // A full packet goes out at once, so one buffer can turn into several packets.
        const auto* in = static_cast<const int16_t*>(input);
        int peak = 0;
        for (unsigned int i = 0; i < frames; ++i) peak = std::max(peak, std::abs((int)in[i]));
        s->micLevel = peak / 32768.f;
        s->quietBuffers = s->micLevel > TalkingLevel ? 0 : std::min(s->quietBuffers + 1, s->holdBuffers);
        s->sending = !s->muted && s->quietBuffers < s->holdBuffers;
        if (s->sending) {
            for (unsigned int i = 0; i < frames; ++i) {
                s->capture->AddInput((AUDIO_SAMPLE)in[i]);
                if (s->capture->sampleCount == AudioData::Capacity) s->SendAudio();
            }
            s->SendAudio();
        }

        // Playback: add up every sender. A ring that runs dry counts as silence.
        size_t n = std::min<size_t>(frames, s->mix.size());
        std::fill(s->mix.begin(), s->mix.begin() + n, 0);
        for (auto& slot : s->slots) {
            if (slot.clear) {
                slot.ring.Clear();
                slot.clear = false;  // only after Clear, so the Update thread can't reuse the slot too early
            }
            size_t popped = slot.ring.Pop(s->scratch.data(), n);
            for (size_t i = 0; i < popped; ++i) s->mix[i] += (int16_t)s->scratch[i];
        }
        auto* out = static_cast<int16_t*>(output);
        for (size_t i = 0; i < frames; ++i) {
            int16_t value = i < n ? (int16_t)std::clamp(s->mix[i], -32768, 32767) : 0;
            out[2 * i] = value;      // left
            out[2 * i + 1] = value;  // right
        }
        return 0;
    }
};

VoiceChatSession::VoiceChatSession(std::function<void(const Event&)> onEvent)
    : onEvent(std::move(onEvent)), capture(std::make_unique<AudioData>()) {
    assert(!g_session && "only one VoiceChatSession can exist per process");
    g_session = this;
}

VoiceChatSession::~VoiceChatSession() {
    Disconnect();
    if (initialized) {
        SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_None, nullptr);
        GameNetworkingSockets_Kill();
    }
    g_session = nullptr;
}

bool VoiceChatSession::Connect(const VoiceChatConfig& newConfig) {
    if (state != SessionState::DISCONNECTED) {
        Log(LogLevel::Error, "already connected");
        return false;
    }
    config = newConfig;
    muted = config.options.muted;
    if (config.name.size() > MaxClientNameLength) {
        Log(LogLevel::Error, Format("name is longer than %u characters", MaxClientNameLength));
        return false;
    }

    if (!initialized) {
        SteamDatagramErrMsg error;
        if (!GameNetworkingSockets_Init(nullptr, error)) {
            Log(LogLevel::Error, Format("GameNetworkingSockets_Init failed: %s", error));
            return false;
        }
        initialized = true;
    }
    // Indexed by LogLevel.
    const ESteamNetworkingSocketsDebugOutputType gnsLevels[] = {
        k_ESteamNetworkingSocketsDebugOutputType_Error, k_ESteamNetworkingSocketsDebugOutputType_Warning,
        k_ESteamNetworkingSocketsDebugOutputType_Msg, k_ESteamNetworkingSocketsDebugOutputType_Debug};
    SteamNetworkingUtils()->SetDebugOutputFunction(gnsLevels[(int)config.logLevel], SessionCallbacks::OnDebugOutput);

    SteamNetworkingIPAddr address;
    if (!address.ParseString(config.address.c_str())) {
        Log(LogLevel::Error, Format("can't parse server address %s", config.address.c_str()));
        return false;
    }
    address.m_port = config.port;

    SteamNetworkingConfigValue_t option;
    option.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)SessionCallbacks::OnStatusChanged);
    ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
    HSteamNetConnection conn = sockets->ConnectByIPAddress(address, 1, &option);
    if (conn == k_HSteamNetConnection_Invalid) {
        Log(LogLevel::Error, "failed to create connection");
        return false;
    }
    if (!ConfigureLanes(sockets, conn)) {
        sockets->CloseConnection(conn, 0, nullptr, false);
        Log(LogLevel::Error, "failed to configure lanes");
        return false;
    }

    connection = conn;
    state = SessionState::CONNECTING;
    deadline = Clock::now() + HandshakeTimeout;
    Log(LogLevel::Info, Format("connecting to %s port %u", config.address.c_str(), (unsigned)config.port));
    return true;
}

void VoiceChatSession::Disconnect() {
    audio.reset();  // stops the stream first, so the audio thread stops sending
    if (connection != k_HSteamNetConnection_Invalid) {
        // Linger so the server sees a clean close, not a timeout.
        SteamNetworkingSockets()->CloseConnection(connection, 0, "client quit", true);
        connection = k_HSteamNetConnection_Invalid;
    }
    peers.clear();
    senders.clear();
    selfId = 0;
    selfTalking = false;
    sending = false;
    micLevel = 0;
    state = SessionState::DISCONNECTED;
}

void VoiceChatSession::Drop(const std::string& reason) {
    Disconnect();
    DrainLogs();  // logs that explain the drop go out before the event
    Fire(Event{EventType::DISCONNECTED, {}, LogLevel::Info, reason});
}

void VoiceChatSession::Update() {
    if (state != SessionState::DISCONNECTED) {
        SteamNetworkingSockets()->RunCallbacks();
        ReceiveMessages();
    }
    if (state == SessionState::CONNECTING && Clock::now() > deadline)
        Drop("timed out waiting for the server");
    if (state == SessionState::CONNECTED)
        UpdateTalking();

    if (droppedSamples) {
        Log(LogLevel::Warning, Format("playback full, dropped %zu samples", droppedSamples));
        droppedSamples = 0;
    }
    if (droppedPackets) {
        Log(LogLevel::Warning, Format("more than %d senders, dropped %zu audio packets", MaxSenders, droppedPackets));
        droppedPackets = 0;
    }
    uint32_t overflows = audioOverflows;
    if (overflows != reportedOverflows) {
        Log(LogLevel::Warning, Format("audio device overflowed or ran dry %u times", overflows - reportedOverflows));
        reportedOverflows = overflows;
    }
    DrainLogs();
}

void VoiceChatSession::UpdateOptions(const VoiceChatOptions& options) {
    // Old-channel audio must not play on the new channel. Senders stay in the
    // map, so their talking state still ends inside Update.
    if (options.channel != config.options.channel)
        for (auto& sender : senders) FreeSlot(sender.first);
    config.options = options;
    muted = options.muted;
    // While connecting, the handshake sends the stored options.
    if (state == SessionState::CONNECTED) SendConfig();
}

void VoiceChatSession::HandleStatus(SteamNetConnectionStatusChangedCallback_t* info) {
    if (info->m_hConn != connection) return;  // late callbacks for a closed connection
    switch (info->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_Connected: {
            // We need the sample rate before we can open the audio stream.
            ServerInfoRequest request;
            Send(connection, request);
            break;
        }
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            Drop(info->m_info.m_szEndDebug);
            break;
        default:
            break;
    }
}

void VoiceChatSession::ReceiveMessages() {
    // A Drop inside HandleMessage resets the connection, which ends the loop.
    while (connection != k_HSteamNetConnection_Invalid) {
        ISteamNetworkingMessage* msg = nullptr;
        if (SteamNetworkingSockets()->ReceiveMessagesOnConnection(connection, &msg, 1) != 1) break;
        if (msg->GetSize() > 0) HandleMessage(static_cast<const uint8_t*>(msg->m_pData), (uint32_t)msg->GetSize());
        msg->Release();
    }
}

void VoiceChatSession::HandleMessage(const uint8_t* bytes, uint32_t size) {
    // static: too big for the stack. Safe because only one session exists.
    static AudioData audioData;
    static ClientList list;
    ServerInfo info;
    ClientJoined joined;
    ClientConfigChanged changed;
    ClientLeft left;
    bool ok = false;

    switch (bytes[0]) {
        case AUDIO:
            if ((ok = Decode(audioData, bytes, size))) HandleAudio(audioData);
            break;
        case SERVER_INFO:
            if ((ok = Decode(info, bytes, size))) HandleServerInfo(info);
            break;
        case CLIENT_LIST:
            if ((ok = Decode(list, bytes, size))) {
                peers.clear();
                for (uint32_t i = 0; i < list.clientCount; ++i) peers.push_back(ToPeer(list.entries[i]));
                Fire(Event{EventType::PEER_LIST});
            }
            break;
        case CLIENT_JOINED:
            if ((ok = Decode(joined, bytes, size))) {
                peers.push_back(ToPeer(joined.entry));
                Fire(Event{EventType::PEER_JOINED, peers.back()});
            }
            break;
        case CLIENT_CONFIG_CHANGED:
            if ((ok = Decode(changed, bytes, size))) {
                Peer peer = ToPeer(changed.entry);
                for (Peer& p : peers)
                    if (p.id == peer.id) p = peer;
                Fire(Event{EventType::PEER_CHANGED, peer});
            }
            break;
        case CLIENT_LEFT:
            if ((ok = Decode(left, bytes, size))) {
                Event event{EventType::PEER_LEFT};
                event.peer.id = left.clientId;
                auto it = std::find_if(peers.begin(), peers.end(), [&](const Peer& p) { return p.id == left.clientId; });
                if (it != peers.end()) {
                    event.peer = *it;
                    peers.erase(it);
                }
                senders.erase(left.clientId);
                FreeSlot(left.clientId);
                Fire(event);
            }
            break;
        default:
            return;
    }
    if (!ok) Log(LogLevel::Warning, Format("dropping malformed message of type %u, %u bytes", bytes[0], size));
}

void VoiceChatSession::HandleServerInfo(const ServerInfo& info) {
    if (state != SessionState::CONNECTING) return;
    if (info.protocolVersion != PROTOCOL_VERSION)
        return Drop(Format("server protocol %u does not match client protocol %u", info.protocolVersion, PROTOCOL_VERSION));
    // Don't trust the rate blindly; buffer sizes come from it.
    if (info.sampleRate < MinSampleRate || info.sampleRate > MaxSampleRate)
        return Drop(Format("server sample rate %u Hz is outside %u to %u Hz", info.sampleRate, MinSampleRate, MaxSampleRate));

    selfId = info.clientId;
    SendConfig();
    if (!StartAudio(info.sampleRate)) return Drop("audio failure");
    state = SessionState::CONNECTED;
    Fire(Event{EventType::CONNECTED});
}

void VoiceChatSession::HandleAudio(const AudioData& packet) {
    uint32_t id = packet.senderId;
    if (id == 0) return;  // the server hands out ids from 1, and 0 marks a free slot
    // Unknown sender: late audio from a client that left, or audio that beat
    // the peer list. Drop it, so a departed peer doesn't come back.
    if (std::none_of(peers.begin(), peers.end(), [&](const Peer& p) { return p.id == id; })) return;

    // This sender's slot, or a free one. A freed slot is reusable only after
    // the audio thread has emptied it.
    PlaybackSlot* slot = nullptr;
    for (PlaybackSlot& s : slots)
        if (s.senderId == id) slot = &s;
    for (PlaybackSlot& s : slots) {
        if (slot) break;
        if (s.senderId == 0 && !s.clear) {
            s.senderId = id;
            slot = &s;
        }
    }
    if (!slot) {
        droppedPackets++;
        return;
    }

    auto [it, isNew] = senders.try_emplace(id);
    SenderAudio& sender = it->second;
    if (!isNew) {
        if (packet.sequence <= sender.lastSequence) return;  // late or duplicate
        // Lost packets become silence, so later audio keeps its timing.
        static const uint16_t silence[AudioData::Capacity] = {};
        uint32_t gap = std::min(packet.sequence - sender.lastSequence - 1, MaxGapPackets);
        for (uint32_t i = 0; i < gap; ++i)
            droppedSamples += packet.sampleCount - slot->ring.Push(silence, packet.sampleCount);
    }
    droppedSamples += packet.sampleCount - slot->ring.Push(packet.Input, packet.sampleCount);
    sender.lastSequence = packet.sequence;
    sender.lastHeard = Clock::now();

    // My own voice coming back (loopback) doesn't count. My talking comes from the mic.
    if (!sender.talking && id != selfId) {
        sender.talking = true;
        SetTalking(id, true);
    }
}

void VoiceChatSession::SendConfig() {
    SetClientConfig message;
    message.channel = config.options.channel;
    message.loopback = config.options.loopback;
    message.muted = config.options.muted;
    snprintf(message.name, sizeof(message.name), "%s", config.name.c_str());
    Send(connection, message);
}

bool VoiceChatSession::StartAudio(unsigned int sampleRate) {
    // 64 bits so a large sync interval can't wrap around.
    uint64_t frames = (uint64_t)sampleRate * config.syncIntervalMs / 1000;
    if (frames < 1) {
        Log(LogLevel::Error, Format("sync interval of %u ms is too short for %u Hz", config.syncIntervalMs, sampleRate));
        return false;
    }
    unsigned int bufferFrames = (unsigned int)frames;

    // RtAudio reports errors here. Without a callback it prints to the console.
    audio = std::make_unique<RtAudio>(RtAudio::UNSPECIFIED, [this](RtAudioErrorType type, const std::string& text) {
        QueueLog(type == RTAUDIO_WARNING ? LogLevel::Warning : LogLevel::Error, text);
    });
    audio->showWarnings(false);

    RtAudio::StreamParameters in, out;
    in.deviceId = audio->getDefaultInputDevice();
    in.nChannels = 1;
    out.deviceId = audio->getDefaultOutputDevice();
    out.nChannels = 2;
    if (audio->openStream(&out, &in, RTAUDIO_SINT16, sampleRate, &bufferFrames, &SessionCallbacks::OnAudio, this)) {
        audio.reset();
        return false;
    }

    // openStream may change bufferFrames, so size everything from what it picked.
    size_t capacity = std::max((size_t)bufferFrames * PlaybackSlackBuffers, (size_t)sampleRate / PlaybackSlackMinFractionOfSecond);
    for (PlaybackSlot& slot : slots) {
        slot.ring.Resize(capacity);
        slot.senderId = 0;
        slot.clear = false;
    }
    scratch.assign(bufferFrames, 0);
    mix.assign(bufferFrames, 0);
    capture->ResetData();
    nextSequence = 0;
    // Round up so the gate holds at least TalkingHold. Start closed.
    uint64_t holdFrames = (uint64_t)sampleRate * TalkingHold.count() / 1000;
    holdBuffers = (uint32_t)((holdFrames + bufferFrames - 1) / bufferFrames);
    quietBuffers = holdBuffers;

    if (audio->startStream()) {
        audio.reset();
        return false;
    }
    return true;
}

void VoiceChatSession::SendAudio() {
    if (capture->sampleCount == 0) return;
    VOICECHAT_AUDIO_STATS_RECORD(capture->Input, capture->sampleCount);
    capture->sequence = nextSequence++;  // senderId stays 0: the server stamps it
    Send(connection, *capture);
    capture->ResetData();
}

void VoiceChatSession::FreeSlot(uint32_t senderId) {
    for (PlaybackSlot& slot : slots) {
        if (slot.senderId != senderId) continue;
        slot.clear = true;
        slot.senderId = 0;
    }
}

void VoiceChatSession::SetTalking(uint32_t id, bool talking) {
    Event event{EventType::TALKING_CHANGED};
    event.peer.id = id;
    for (Peer& peer : peers) {
        if (peer.id != id) continue;
        peer.talking = talking;
        event.peer = peer;
    }
    event.peer.talking = talking;
    Fire(event);
}

void VoiceChatSession::UpdateTalking() {
    auto now = Clock::now();
    bool talking = sending;  // the audio thread's silence gate
    if (talking != selfTalking) {
        selfTalking = talking;
        SetTalking(selfId, talking);
    }

    for (auto it = senders.begin(); it != senders.end();) {
        SenderAudio& sender = it->second;
        if (sender.talking && now - sender.lastHeard > TalkingHold) {
            sender.talking = false;
            SetTalking(it->first, false);
        }
        if (now - sender.lastHeard > SenderTimeout) {
            FreeSlot(it->first);
            it = senders.erase(it);
        } else {
            ++it;
        }
    }
}

void VoiceChatSession::Fire(const Event& event) {
    if (onEvent) onEvent(event);
}

void VoiceChatSession::Log(LogLevel level, const std::string& text) {
    if (level <= config.logLevel) Fire(Event{EventType::LOG, {}, level, text});
}

void VoiceChatSession::QueueLog(LogLevel level, const std::string& text) {
    std::lock_guard<std::mutex> lock(logMutex);
    queuedLogs.emplace_back(level, text);
}

void VoiceChatSession::DrainLogs() {
    std::vector<std::pair<LogLevel, std::string>> logs;
    {
        std::lock_guard<std::mutex> lock(logMutex);
        logs.swap(queuedLogs);
    }
    for (auto& log : logs) Log(log.first, log.second);
}
