# Phase 2 implementation plan: VoiceChatCore client library

Status: done 2026-11-13 (deviations in the phase doc's "What was done"). This plan goes with [phase-2-core-library.md](phase-2-core-library.md). Three chunks, run in order. Each chunk leaves the build green.

## Decisions (made by the user)

1. **Runtime options**: one `VoiceChatOptions` struct (`channel`, `muted`, `loopback`) and one `UpdateOptions(opts)` call. Core keeps the current options and resends the full `SetClientConfig` on any change. A channel change clears playback.
2. **Audio transport**: `AUDIO` goes `k_nSteamNetworkingSend_UnreliableNoNagle` on the client upstream and on the server relay. Everything else stays `ReliableNoNagle`. Every connection has two GNS lanes: lane 0 audio, lane 1 control. Each side configures lanes for what it sends. The audio message gets a per-sender `sequence`; playback drops late or duplicate packets and fills gaps with silence. `PROTOCOL_VERSION` becomes 2.
3. **GNS callback routing**: one live session per process, in a file-static `VoiceChatSession*`. The constructor asserts if a second one exists.
4. **Own client id**: `ServerInfo` gains `clientId u32`, in the same version 2 bump. Core exposes `SelfId()`.
5. **Mixing is in scope.** Each sender gets its own playback ring. The audio callback mixes them.
6. **Own log enum.** Core defines `LogLevel`. GNS types stay out of Core's public API.
7. **One small test executable** for `SampleRing` (asserts, no framework). CI runs it.
8. Event API: one `std::function<void(const Event&)>` and a flat `Event` struct (maps to one C callback in phase 4).
9. Phase 1.5 leftovers are skipped. The `k_HSteamNetPollGroup_Invalid` server fix is folded into chunk 1.

Plan-level choices (local, reversible): one send helper picks lane and reliability from wire byte 0. `AUDIO_SAMPLE` stays `uint16`; level and mix math cast to `int16_t`.

## GNS 1.6.1 lane API (checked against installed headers)

- `EResult ISteamNetworkingSockets::ConfigureConnectionLanes(HSteamNetConnection, int nNumLanes, const int* pLanePriorities, const uint16* pLaneWeights)`. `nullptr` = equal priority / equal weight. Call any time after the handle exists. Lane count can't shrink.
- Lanes are per direction, so both sides call it: the client right after `ConnectByIPAddress`, the server right after `AcceptConnection`.
- `SendMessageToConnection` always uses lane 0. For lane 1: `SteamNetworkingUtils()->AllocateMessage(size)`, fill `m_pData`, `m_conn`, `m_nFlags`, `m_idxLane`, then `SendMessages(1, &msg, nullptr, true)`.
- GNS advises the most common traffic on lane 0, hence audio on lane 0.
- Choice: `ConfigureConnectionLanes(conn, 2, nullptr, nullptr)`.

## Wire change (chunk 1)

| Message | Fields in serialize order | Wire bytes |
|---|---|---|
| `AudioData` (v2) | type u8 (byte 0), senderId u32 (bytes 1–4, stamped by server), **sequence u32 (bytes 5–8, set by client, server doesn't touch it)**, count u32 (bytes 9–12, ≤ 1024), samples 16 bits each | **13 + 2·count** |
| `ServerInfo` (v2) | existing fields + **clientId u32** (the receiving client's own id) | +4 |

- `sequence` counts sent audio packets per connection from 0. The client increments it only when it sends, so mute makes no gap. u32 wraps after ~248 days at 5 ms; no wrap handling.
- Server relay, `AUDIO` case in `VoiceChatServer/Server.cpp`: reject if `size < 13`; `memcpy(&count, bytes + 9, 4)`; valid if `count <= AudioData::Capacity && 13 + 2 * count == size`; patch stays `memcpy(bytes + 1, &senderId, 4)`. `AudioData::MaxWireSize = 13 + 2 * Capacity`.

## Final file list

```
VoiceChatCore/
  CMakeLists.txt
  VoiceChatSession.h     public API (the only header consumers include)
  VoiceChatSession.cpp   GNS connection, handshake, messages, RtAudio callback, mixing, talking, logging
  SampleRing.h           lock-free SPSC ring (header-only, std only)
  SampleRingTest.cpp     assert-based check, own executable
VoiceChatClient/
  CMakeLists.txt, main.cpp, Config.h, Config.cpp     (kept, rewritten or trimmed)
  deleted: SocketClient.{h,cpp}, AudioTools.{h,cpp}, Utils.h, ConcurrentBag.hpp
Common/Messages/MessageTypes.h      + ConfigureLanes(), SendToConnection()
Common/Messages/AudioMessage.h      + sequence, new MaxWireSize, drop printf in AddInput
Common/Messages/ServerInfoMessage.h + clientId
```

No new `Common/` header, so the Common file lists only change by adding Core's own list.

## Public header: `VoiceChatCore/VoiceChatSession.h`

Shape to aim for. Keep GNS headers out of it: hold the connection as `std::atomic<uint32_t>` (`HSteamNetConnection` is a `uint32` typedef) and forward-declare the rest.

```cpp
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
// Events fire only inside Update(), on that thread.
class VoiceChatSession {
public:
    explicit VoiceChatSession(std::function<void(const Event&)> onEvent);
    ~VoiceChatSession();                        // Disconnect, then GameNetworkingSockets_Kill

    bool Connect(const VoiceChatConfig& config); // false = failed at once; a LOG event says why
    void Disconnect();                           // stop audio, close the connection; no event
    void Update();                               // pump network + queued logs, fire events
    void UpdateOptions(const VoiceChatOptions& options);

    SessionState State() const;
    const VoiceChatOptions& Options() const;
    const std::vector<Peer>& Peers() const;      // all channels; filter yourself
    uint32_t SelfId() const;                     // 0 until connected
    float MicLevel() const;                      // 0..1, peak of the last mic buffer
    // private: see threading model below
};
```

`RtAudio` and `AudioData` are forward-declared and held in `unique_ptr`, so consumers need neither RtAudio nor `Common/` headers. The destructor is defined in the .cpp.

## Threading model

| Thread | Does | Touches |
|---|---|---|
| **Caller** (calls `Update`) | `RunCallbacks` (status callback → `Drop`), `ReceiveMessagesOnConnection`, decode, sequence check, slot assignment, `Push` into sender rings, client list, handshake state machine + 30 s timeout, talking hold timers, drains queued logs, fires every event, sends control messages, starts/stops RtAudio, `UpdateOptions` | Update-thread members; stores the shared atomics |
| **RtAudio callback** (realtime) | Honors per-slot clear flags. Mic peak → `micLevel`. If not `muted`: capture, `sequence = nextSequence++`, encode, `SendToConnection` (a full packet goes out at once, as today). Playback: mix every slot (see below), **zero-fill whatever is missing (fixes the underrun defect)**. Non-zero `status` → `audioOverflows++`. Never locks, never logs. | atomics, slot rings (consumer), audio-only members |
| **GNS internal** | `OnDebugOutput` → `QueueLog` (mutex) | queued logs |
| **RtAudio error callback** | Pass an error callback to the `RtAudio` constructor that calls `QueueLog`. Without it RtAudio writes to `std::cerr`. Also `showWarnings(false)`. | queued logs |

- Atomics: `connection`, `muted`, `micLevel`, `audioOverflows`, per-slot `senderId` and `clear`, the rings' `head`/`tail`.
- `Update` compares `audioOverflows` with what it last reported and logs the difference once.
- GNS sends are thread-safe; a send on a stale or closed handle returns an error. `StopAudio()` still runs before `CloseConnection`.

## Playback: per-sender rings and mixing

```cpp
// ponytail: fixed slot count; raise it if channels get bigger than this
constexpr int MaxSenders = 8;

struct PlaybackSlot {
    std::atomic<uint32_t> senderId{0};  // 0 = free. Written by the Update thread.
    std::atomic<bool> clear{false};     // Update thread sets it, audio thread empties the ring and resets it
    SampleRing ring;                    // producer: Update thread, consumer: audio thread
};
std::array<PlaybackSlot, MaxSenders> slots;
```

Check that the server never hands out client id 0 (if it can, use a separate `used` flag instead).

- **Find a slot (Update thread)**: the slot whose `senderId` matches. Otherwise a free one with `senderId == 0 && !clear` (a freed slot is reusable only after the audio thread finished clearing it). No slot free: drop the packet and log once.
- **Free a slot**: when the peer leaves, when the sender has been silent for 2 s, or for all slots on a channel change. Set `clear = true`, then `senderId = 0`.
- **Audio thread mix**: for each slot, `if (clear.exchange(false)) ring.Clear();`. Then pop up to `frames` samples into a per-slot scratch and add them as `int16_t` into an `int32_t` mix buffer. Missing samples count as zero. Clamp to int16 and write the same value to left and right.
- Ring capacity per slot: `max(frames * 4, rate / 10)` (the constants from `Utils.h`). Resize all slots before the stream starts.

`HandleAudio` (Update thread), with the sender's state in a `std::map<uint32_t, SenderAudio>` (`lastSequence`, `lastHeard`):
- Unknown sender: add it, push.
- `sequence <= lastSequence`: drop (late or duplicate).
- `sequence > last + 1`: push `min(sequence - last - 1, MaxGapPackets) * count` zeros, then the samples. `constexpr uint32_t MaxGapPackets = 2;` with `// ponytail: silence is sized from this packet's count; a jitter buffer would time it properly`.
- Accepted: update `lastSequence`, `lastHeard`; if the peer isn't talking yet, set `talking = true` and fire `TALKING_CHANGED`.
- If `Push` returned less than asked, count the dropped samples and log once per `Update`.
- Decode into a function-local `static AudioData` (safe because of the one-session rule; leave a comment).

## Talking

- Peers: `talking && now - lastHeard > 200 ms` → `talking = false`, fire `TALKING_CHANGED`.
- Self: `loud = !muted && MicLevel() > TalkingLevel`, `constexpr float TalkingLevel = 0.02f;` (calibration knob, about −34 dBFS peak). Same 200 ms hold. Event peer id is `SelfId()`.
- Mic peak: `abs((int16_t)sample) / 32768.f`.

## Client list events

- `CLIENT_LIST` replaces the peer list → `PEER_LIST`.
- `CLIENT_JOINED` appends → `PEER_JOINED`.
- `CLIENT_CONFIG_CHANGED` updates the entry (keeps `talking`) → `PEER_CHANGED`.
- `CLIENT_LEFT` copies the peer, erases it from peers and senders, frees its slot → `PEER_LEFT`.

## Session lifecycle

- **Constructor**: `assert(!g_session); g_session = this;`
- **`Connect`**: `GameNetworkingSockets_Init` once; `SetDebugOutputFunction` with the GNS level mapped from `LogLevel`; `addr.ParseString(address)` + `m_port` (drops `inet_pton` and Winsock includes); `ConnectByIPAddress` with the status callback option; `ConfigureLanes`; state `CONNECTING`; set the 30 s deadline.
- **Status callback**: `Connected` sends `ServerInfoRequest`. `ClosedByPeer` / `ProblemDetectedLocally` → `Drop(m_szEndDebug)`.
- **`SERVER_INFO`**: check protocol and sample-rate range (move `MinSampleRate`/`MaxSampleRate` here from `Config.h`); store `clientId`; `frames = rate * syncIntervalMs / 1000` (≥ 1); resize slot rings; `SendConfig()`; `StartAudio`; state `CONNECTED`; fire `CONNECTED`. Any failure → `Drop(reason)`.
- **`Drop`**: `StopAudio`; `CloseConnection(conn, 0, nullptr, false)`; connection = invalid; clear peers, senders, slots; state `DISCONNECTED`; fire `DISCONNECTED{text = reason}`. Runs inside `RunCallbacks`, so never calls `GameNetworkingSockets_Kill`.
- **`Disconnect`**: same as `Drop`, no event.
- **Destructor**: `Disconnect()`; `SetDebugOutputFunction(None, nullptr)`; `GameNetworkingSockets_Kill()`; `g_session = nullptr`.
- Delete the non-open-source `#else` branch (`SteamDatagram_SetAppID`…). Dead code.
- **`UpdateOptions(o)`**: if the channel changed, free all slots and clear senders. Store options, `muted = o.muted`. If `CONNECTED`, `SendConfig()`; while connecting, the handshake sends the stored options.

## SPSC ring: `VoiceChatCore/SampleRing.h`

```cpp
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

// Lock-free sample queue for exactly one producer thread and one consumer thread.
// head and tail count samples ever written/read; index = count % capacity.
class SampleRing {
public:
    // Only while no other thread uses the ring (before the audio stream starts).
    void Resize(size_t capacity) { data.assign(capacity, 0); head = 0; tail = 0; }

    size_t Size() const { return (size_t)(head.load(std::memory_order_acquire) - tail.load(std::memory_order_acquire)); }

    // Producer. Returns how many samples fit; the rest are dropped (newest lost when full).
    size_t Push(const uint16_t* in, size_t n) {
        uint64_t h = head.load(std::memory_order_relaxed);
        uint64_t t = tail.load(std::memory_order_acquire);
        n = std::min(n, data.size() - (size_t)(h - t));
        for (size_t i = 0; i < n; ++i) data[(h + i) % data.size()] = in[i];
        head.store(h + n, std::memory_order_release);
        return n;
    }

    // Consumer.
    size_t Pop(uint16_t* out, size_t n) {
        uint64_t t = tail.load(std::memory_order_relaxed);
        uint64_t h = head.load(std::memory_order_acquire);
        n = std::min(n, (size_t)(h - t));
        for (size_t i = 0; i < n; ++i) out[i] = data[(t + i) % data.size()];
        tail.store(t + n, std::memory_order_release);
        return n;
    }

    // Consumer only. Drops everything queued.
    void Clear() { tail.store(head.load(std::memory_order_acquire), std::memory_order_release); }

private:
    std::vector<uint16_t> data;
    std::atomic<uint64_t> head{0};
    std::atomic<uint64_t> tail{0};
};
```

- Gap fill pushes from a `static const uint16_t silence[AudioData::Capacity] = {}` in a loop. No extra method.
- Policy change: when full, the newest samples drop (the old buffer dropped the oldest). An SPSC producer can't move the consumer's tail; the cap bounds latency either way.
- `SampleRingTest.cpp`: push/pop round trip, wrap-around past capacity, push into a full ring returns the fitting count, `Clear` empties it, and one two-thread run (producer pushes a counting sequence, consumer checks order). Plain `assert`; return 0. Make sure asserts stay on in the build that CI runs (`#undef NDEBUG` at the top of the file).

## Lane and send helpers (`Common/Messages/MessageTypes.h`, chunk 1)

```cpp
// Each side sets up lanes for what it sends. Lane 0 carries audio (most of
// the traffic; GNS advises lane 0 for that). Lane 1 carries requests and
// events, so audio and control never wait behind each other.
inline bool ConfigureLanes(ISteamNetworkingSockets* sockets, HSteamNetConnection conn) {
    return sockets->ConfigureConnectionLanes(conn, 2, nullptr, nullptr) == k_EResultOK;
}

// Audio goes unreliable on lane 0, everything else reliable on lane 1.
// The type byte decides, so callers pass encoded bytes.
inline void SendToConnection(ISteamNetworkingSockets* sockets, HSteamNetConnection conn,
                             const void* bytes, uint32 size) {
    bool audio = ((const uint8_t*)bytes)[0] == AUDIO;
    SteamNetworkingMessage_t* msg = SteamNetworkingUtils()->AllocateMessage(size);
    memcpy(msg->m_pData, bytes, size);
    msg->m_conn = conn;
    msg->m_nFlags = audio ? k_nSteamNetworkingSend_UnreliableNoNagle : k_nSteamNetworkingSend_ReliableNoNagle;
    msg->m_idxLane = audio ? 0 : 1;
    sockets->SendMessages(1, &msg, nullptr, true);
}
```

If `SteamNetworkingUtils()` isn't visible, add `#include <steam/isteamnetworkingutils.h>`.

## CMake

**Root**: Core needs RtAudio (the `client` feature), so it builds only with the client switch:
```cmake
if(VOICECHAT_BUILD_CLIENT)
    add_subdirectory(VoiceChatCore)
    add_subdirectory(VoiceChatClient)
endif()
```

**`VoiceChatCore/CMakeLists.txt`**: `find_package(rtaudio CONFIG REQUIRED)`; `add_library(VoiceChatCore STATIC ...)` listing its own files plus every `Common/` header it uses; `target_include_directories(... PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`; link `${GNS_LIB}` and `RtAudio::rtaudio` (PRIVATE if the public header stays free of GNS types). `add_executable(SampleRingTest SampleRingTest.cpp)`.

**`VoiceChatClient/CMakeLists.txt`** (chunk 3): `main.cpp Config.cpp Config.h` (+ `AudioStats.h` if still used), link `VoiceChatCore cxxopts::cxxopts`.

**CI**: both workflows run `SampleRingTest` after the build.

The "Common headers in both CMakeLists" rule now means `VoiceChatServer/` and `VoiceChatCore/`. No `vcpkg.json` change.

## Build command

`cmd /c "call \"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat\" && set VCPKG_ROOT=C:\Work\OpenSource\vcpkg && cmake --preset debug && cmake --build --preset debug"`

## Chunks

### Chunk 1: wire v2, lanes, unreliable audio, own id, server fix

1. `MessageTypes.h`: `PROTOCOL_VERSION = 2`; add `ConfigureLanes`, `SendToConnection`.
2. `AudioMessage.h`: `uint32 sequence` serialized right after `senderId`; `MaxWireSize = 13 + 2 * Capacity`; update the wire comment; delete the `printf` in `AddInput` and the `stdio.h` include.
3. `ServerInfoMessage.h`: add `clientId`.
4. `Server.cpp`: `SendMessage` body → `SendToConnection(...)` (keep the byte counter). After `SetConnectionPollGroup` in `Connecting`: `if (!ConfigureLanes(...))` close, log, `break`, before adding to `clients`. Fill `clientId` in the server-info reply. `AUDIO` case: new offsets (13 / bytes 9–12), update its comment. `StartServer`: the second check (`k_HSteamNetPollGroup_Invalid`) duplicates the one above once corrected to `k_HSteamListenSocket_Invalid` — delete it.
5. Current client (throwaway, keeps the build and wire working): `ConfigureLanes` after `ConnectByIPAddress`; `Send` → `SendToConnection`; `AudioTools.cpp` sets `sequence` from a counter before encoding.
6. Docs: `CLAUDE.md` Rules (relay offsets: reads `count` from bytes 9–12), `phase-1-protocol-presence.md` notes (`AudioData` 13 + 2·count, `ServerInfo` clientId, version 2), `technical-design.md` transport (unreliable audio, two lanes), `README.md` tick the small fix.

Verify: build; server + `VoiceChatClient --loopback` (hear yourself); two clients hear each other.

### Chunk 2: VoiceChatCore library (nothing uses it yet)

1. `SampleRing.h`, `SampleRingTest.cpp`.
2. `VoiceChatSession.h`, `VoiceChatSession.cpp` per the sections above. Port from `SocketClient.cpp`, `AudioTools.cpp`, `main.cpp`. `Log` uses `vsnprintf` into `char[512]` and emits `LOG`. RtAudio setup as today (duplex, mono in / stereo out, `RTAUDIO_SINT16`, `RTAUDIO_HOG_DEVICE | RTAUDIO_SCHEDULE_REALTIME`, `userData = this`). Keep `VOICECHAT_AUDIO_STATS_RECORD` in the send path. Zero `printf`/`cout`/`cerr`.
3. `VoiceChatCore/CMakeLists.txt`, root `add_subdirectory`, CI step for `SampleRingTest`.
4. Don't touch `VoiceChatClient`.

Verify: build; run `SampleRingTest`; server-only configure (`-DVOICECHAT_BUILD_CLIENT=OFF`, separate build dir) still builds; `grep -rnE "(^|[^a-z])printf|cout|cerr|puts\(" VoiceChatCore` finds nothing outside the test.

### Chunk 3: CLI on Core, delete old files, smoke test, docs

1. `Config.h/.cpp`: delete `ClientConfig`; `ConfigParser::Parse(argc, argv, VoiceChatConfig&)`; map the log-level flag to `LogLevel`; delete `MinSampleRate`/`MaxSampleRate`.
2. `main.cpp` rewrite (~80 lines): print events; `Connect` or exit 1; SIGINT sets `g_quit`; a detached stdin thread queues lines into a mutex-guarded deque (`m` mute, `l` loopback, `c <n>` channel, `q` quit); main loop applies commands via `UpdateOptions`, calls `Update`, sleeps `syncIntervalMs`; exit on quit or `DISCONNECTED`; `Disconnect()`; `VOICECHAT_AUDIO_STATS_PRINT()`; return 1 if dropped.
3. CLI `CMakeLists.txt`; delete `SocketClient.{h,cpp}`, `AudioTools.{h,cpp}`, `Utils.h`, `ConcurrentBag.hpp`.
4. Docs: `phase-2-core-library.md` "Notes for later phases" (header path, final surface, "call Update every syncIntervalMs", "one session per process"); `technical-design.md` (client section, threads, known limits); `CLAUDE.md` layout (add `VoiceChatCore/`, Common-headers rule names server and Core, `SampleRingTest` under Testing); `README.md` status: phase 2 done; Serena memories that mention the relay offsets or the static client.

## Smoke test (end of chunk 3)

1. Server, then `VoiceChatClient --name a --loopback`: hear yourself; self talking lines print.
2. Second client `b`: both print the list and join; both hear each other.
3. Both talk at once: both voices are audible, not garbled, and latency doesn't grow (mixing).
4. `m` in `a`: `b` stops hearing `a`; server logs `muted 1`; `b` prints a peer change. `m` again resumes.
5. `l`, then `c 1` in `a`: server logs each change; `b` gets `CLIENT_CONFIG_CHANGED`; after `c 1` they don't hear each other; `c 0` restores without old-channel audio.
6. Ctrl-C in `a`: exit code 0; server logs "closed by peer", not a timeout; `b` prints the leave.
7. Kill the server while `b` runs: `b` prints "disconnected: …" and exits 1.

## Risks

- **Clean close vs `GameNetworkingSockets_Kill`.** If step 6 shows a server timeout: in `Disconnect`, run `RunCallbacks` + short sleep (~100 ms) before returning, or close with linger and wait. Pick one.
- **MTU.** Unreliable messages over ~1.2 KB fragment. Default 5 ms at 44.1 kHz is ~453 bytes. Large `--sync-interval` values still work but are more loss-sensitive.
- **`Update` cadence drives playback.** Phase 3's loop must call it about every `syncIntervalMs`.
- **Static decode buffers** are safe only because of the one-session rule.

## Skipped on purpose

- No sent/received byte counters in Core. Add when phase 3's status bar needs them.
- No jitter buffer.
