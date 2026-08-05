# Phase 1 implementation plan: wire serialization + protocol restructure + server presence

Status: executed 2026-08-04. Naming changed after execution: "roster" became "client list" (`RosterMessages.h` → `ClientListMessages.h`, `RosterList` → `ClientList`, `RosterEntry` → `ClientEntry`, `ROSTER_LIST` → `CLIENT_LIST`); read the roster names below with that mapping. Companion to [phase-1-protocol-presence.md](phase-1-protocol-presence.md); this doc records the design decisions made during planning and the exact steps to implement.

## Context

The terminal client (phase 3) needs roster, online count, and "who is talking" — none of which the protocol can express today. This phase delivers the roadmap's phase-1 scope (message categories, server-assigned client ids and names, roster events, sender id on audio, `muted` flag, strict version handshake) **plus a decision made during planning that supersedes the roadmap's "raw POD structs" rule**: the wire format moves from memcpy'd structs to explicit field-by-field serialization over a bit-level stream (the unified-serialize pattern from Glenn Fiedler's "Building a Game Network Protocol" series). Reasons: struct casting made the wire format compiler/ABI-dependent; explicit little-endian encoding makes cross-platform layout correct by construction; the bit-level core plus per-field helpers create the seam for future audio compression without implementing any now. Both changes touch every message site, so they land together under one version bump.

## Design decisions

- **Serialization: unified `Serialize(Stream&)` per message** — one field list serves write, read, and measure via `if constexpr (Stream::IsWriting)` (C++17, available). Streams wrap a bit-level writer/reader (64-bit scratch, flushed as little-endian 32-bit words). Today every field is written at a byte-multiple width, so wire bytes equal a byte-level implementation — the bit core exists so later compression is a localized change.
- **Explicit little-endian, big-endian unsupported**: enforced with a preprocessor guard (`#error` on big-endian) — C++17 has no `std::endian`, and we don't bump the standard for one check.
- **Every count/length range-checked on read** (`serialize_*_range` fails the stream). GNS authenticates peers but a modified client is still a valid peer; payload is untrusted. This replaces today's ad-hoc validation (client audio checks `SocketClient.cpp:163-179`, server config size check) and fixes the server's currently *unvalidated* audio relay and the client's unchecked `SERVER_INFO` read.
- **First serialized byte stays the type discriminator** — both dispatch switches keep peeking byte 0 before decoding.
- **One plain `enum MessageType` grouped by range** (requests 1–63, events 64–127, data 128+). Old values 2 and 3 stay unassigned so a pre-phase-1 client's first byte lands in `default` and gets a clear rejection.
- **Client id: `uint32`, monotonic from 1**, assigned where the `clients` map entry is created (accept time, `Server.cpp:195-199`). 0 = unassigned (what clients send in upstream audio).
- **Audio sender id: server-stamped, unspoofable.** Client writes 0; server overwrites before relay. Wire offset of `senderId` is fixed by the serializer (bytes 1–4, LE), so the server patches those 4 bytes in the received buffer once and relays the same buffer to all recipients — no per-recipient re-encode on the hot path.
- **Version handshake: `protocolVersion` field in both client requests** (`ServerInfoRequest` — the actual first message — and `SetClientConfig`). Strict equality; bumped on ANY wire change including future encoding changes. Server rejects via `CloseConnection(conn, k_ESteamNetConnectionEnd_App_Generic, "server protocol X, client protocol Y", false)`; the reason string rides in the close packet and the client callback already prints it (`SocketClient.cpp:71-80`). `ServerInfo` echoes the server's version. No extra handshake state: `AUDIO` is already gated on `hasConfig`, which only becomes true through a version-checked config.
- **Roster snapshot: one variable-length `RosterList` event** (count + entries; count doubles as online count), sent to a client when its first config is accepted. Join/leave/config-changed events broadcast to all configured clients; entries carry the channel and clients filter locally (per-channel scoping would need synthetic join/leave on channel switch).
- **Names: 32 bytes fixed on wire (`MaxClientNameLength = 31`)**, client `--name` (`-n`) defaults to empty, server substitutes `client-<id>` and always forces termination.
- **Event emission:** `CLIENT_JOINED` on `hasConfig` false→true; `CLIENT_CONFIG_CHANGED` on later configs (broadcast including sender); `CLIENT_LEFT` in the disconnect callback **before** the existing `clients.erase` (`Server.cpp:154-156`), only if the client had config.
- **Audio stats instrumentation, compiled out by default** (`VOICECHAT_AUDIO_STATS`): element count per packet + bits-required histogram over **zig-zag-mapped int16** values (samples are signed PCM stored in uint16 — RtAudio uses `RTAUDIO_SINT16`; an unsigned histogram would make every negative sample look like 16 bits). Data feeds the later compression decision; none implemented now.

## Step 1 — Serialization layer (new `Common/Serialization/`)

### `BitStream.h` — bit-level core

```cpp
// Writes bits LSB-first into a 64-bit scratch word, flushed as
// little-endian 32-bit words. On this scheme, fields written at
// byte-multiple widths land as plain little-endian bytes.
class BitWriter {
public:
    BitWriter(void* buffer, uint32_t bytes);
    void WriteBits(uint32_t value, int bits);   // bits in [1,32]
    void FlushBits();                            // flush final partial word
    uint32_t BytesWritten() const;               // ceil(bitsWritten / 8)
    bool Error() const;                          // overflow attempted
};

class BitReader {
public:
    BitReader(const void* buffer, uint32_t bytes);
    uint32_t ReadBits(int bits);                 // returns 0 and sets error past end
    bool Error() const;
};
```

Implementation care points: the writer's buffer works in whole 32-bit words internally, but `BytesWritten()` is exact — GNS messages have arbitrary byte length. The reader must tolerate a byte length that is not a multiple of 4 (read the tail bytes into the scratch safely rather than over-reading).

### `Streams.h` — three stream types over the core

```cpp
struct WriteStream   { static constexpr bool IsWriting = true;  BitWriter writer; ... };
struct ReadStream    { static constexpr bool IsWriting = false; BitReader reader; ... };
struct MeasureStream { static constexpr bool IsWriting = true;  uint32_t bits = 0; ... }; // counts, never writes
```

Each exposes `SerializeBits(uint32_t& value, int bits)` and an `Error()`/failure flag; the helpers below are the only intended call surface.

### `Serialize.h` — field helpers + endian guard

```cpp
template<typename S> bool serialize_uint8 (S&, uint8_t&);
template<typename S> bool serialize_uint16(S&, uint16_t&);
template<typename S> bool serialize_uint32(S&, uint32_t&);
template<typename S> bool serialize_int64 (S&, int64_t&);          // two 32-bit halves
template<typename S> bool serialize_uint32_range(S&, uint32_t&, uint32_t min, uint32_t max); // read fails outside range
template<typename S> bool serialize_name(S&, char* buf);           // fixed 32 bytes, forces buf[31] = 0 on read

// The audio sample array's encoding lives here and only here. Future
// candidates, none implemented now — measure real traffic first
// (VOICECHAT_AUDIO_STATS): narrower fixed width, per-value size-class
// tags, presence bitmask, delta vs previous packet, codec (Opus).
template<typename S> bool serialize_sample_array(S&, uint16_t* samples, uint32_t& count, uint32_t capacity);
```

`serialize_sample_array` serializes `count` (range-checked against capacity) then the samples at 16 bits each. The endian guard (`#error` on big-endian) lives at the top of this header.

### `AudioStats.h` — instrumentation, compiled out by default

No-op macros unless `VOICECHAT_AUDIO_STATS` is defined: record per-packet sample count and a 17-bucket bits-required histogram of zig-zag-mapped int16 samples; print on shutdown. Hooked in the client's audio send path only. Not added to release builds or CMake defaults.

Both `CMakeLists.txt` files get the new headers (they list sources explicitly — server line 15, client line 18).

## Step 2 — Messages (`Common/Messages/`)

Structs stay as plain in-memory types (no packing pragmas, no wire-sizeof asserts — the serializer defines the wire format). Each gains `template<typename Stream> bool Serialize(Stream&)` writing `type` first. Wire layouts (exact bytes, no padding):

### `MessageTypes.h` (rewrite)

```cpp
constexpr uint32 PROTOCOL_VERSION = 1;   // bump on ANY wire change, including field encodings
constexpr uint32 MaxClientNameLength = 31;

// One byte on the wire. The value's range tells you the category:
//   1-63 requests (client->server), 64-127 events (server->client), 128+ data (both).
// Values 2 and 3 stay unused so pre-versioning clients never alias a current message.
enum MessageType {
    SET_CLIENT_CONFIG = 1,  GET_SERVER_INFO = 4,                      // requests
    SERVER_INFO = 64, ROSTER_LIST = 65, CLIENT_JOINED = 66,           // events
    CLIENT_LEFT = 67, CLIENT_CONFIG_CHANGED = 68,
    AUDIO = 128,                                                      // data
};
```

### Wire formats

| Message | Fields in serialize order | Wire bytes |
|---|---|---|
| `SetClientConfig` | type u8, protocolVersion u32, channel i64, loopback u8, muted u8, name 32B | 47 |
| `ServerInfoRequest` | type u8, protocolVersion u32 | 5 |
| `ServerInfo` | type u8, sampleRate u32, protocolVersion u32 | 9 |
| `AudioData` | type u8, senderId u32 (bytes 1–4, server-stamped), samples via serialize_sample_array (count u32 range≤1024, then 16-bit samples) | 9 + 2·count |
| `RosterEntry` (nested) | channel i64, clientId u32, muted u8, name 32B | 45 |
| `RosterList` | type u8, clientCount u32 (range ≤ 64), entries | 5 + 45·count |
| `ClientJoined` / `ClientConfigChanged` | type u8, RosterEntry | 46 |
| `ClientLeft` | type u8, clientId u32 | 5 |

- `AudioMessage.h`: `AudioData` keeps `Capacity`, `AddInput`, `ResetData` (RtAudio callback untouched); drops `WireSize()`/`HeaderSize()`/offsetof; gains `senderId` + `Serialize`. A comment on `senderId` documents the fixed wire offset the server patches.
- New `RosterMessages.h`: `RosterEntry` (with its own `Serialize`, reused by the three events), `RosterList` (in-memory `entries[64]`), `ClientJoined`, `ClientConfigChanged`, `ClientLeft`.
- Each message exposes `static constexpr uint32 MaxWireSize` (or a shared helper computes it via `MeasureStream`) so send paths can use stack staging buffers.

## Step 3 — Server (`VoiceChatServer/`) — builds at end of step

### `Server.h`

- `ClientInfo` gains `uint32 clientId = 0;`, `bool muted = false;`, `std::string name;`. `hasConfig` comment: also means the protocol version checked out.
- New private statics (matching the existing static-state pattern):
  - `static uint32 nextClientId;` (define `= 1`)
  - `static void SendMessage(HSteamNetConnection conn, const void* bytes, uint32 size);` — wraps `SendMessageToConnection(..., k_nSteamNetworkingSend_ReliableNoNagle, nullptr)` + counts sent bytes; replaces the raw send sites.
  - Per-case stack-buffer encode + `SendMessage` (local `uint8_t buf[MaxWireSize]`, `WriteStream`, `msg.Serialize`, send `BytesWritten()`).
  - `static void BroadcastToConfigured(const void* bytes, uint32 size, HSteamNetConnection skip);`
  - `static void FillRosterEntry(RosterEntry& out, const ClientInfo& info);`
  - `static void RejectClient(HSteamNetConnection conn, const char* reason);` — printf, `CloseConnection(conn, k_ESteamNetConnectionEnd_App_Generic, reason, false)`, `clients.erase(conn)` (a locally initiated close never re-enters the disconnect callback). Callers drop their iterators after calling it.

### `Server.cpp`

- **Accept path** (~line 195): assign `clients[conn].clientId = nextClientId++;`, log the id.
- **Disconnect path** (~line 154): before the existing erase, if `hasConfig`: encode `ClientLeft`, log, `BroadcastToConfigured(..., skip = the leaver)`.
- **`PollIncomingMessages`** — add the missing `GetSize() != 0` guard before reading byte 0, then per case decode with `ReadStream` into a local struct:
  - `SET_CLIENT_CONFIG`: decode failure or version mismatch → `RejectClient` with snprintf'd reason (`"malformed SetClientConfig"` / `"server protocol %u, client protocol %u"`). Else copy channel/loopback/muted, sanitize name (empty → `client-<id>`), `firstConfig` check:
    - first config → encode `RosterList` from all configured clients (warn if > 64) and send to the new client; encode `ClientJoined` and broadcast to everyone else.
    - later config → encode `ClientConfigChanged`, broadcast to all including sender.
  - `AUDIO`: sender must exist and have `hasConfig`. Validate without a full decode on the hot path: size ≥ 9, and `count` (bytes 5–8 LE) satisfies `9 + 2*count == GetSize()` and `count ≤ Capacity` — drop + log otherwise. Then patch bytes 1–4 with the sender's id (LE) in the received buffer and relay via `SendMessage` (existing channel/loopback filter loop unchanged).
  - `GET_SERVER_INFO`: decode failure → `RejectClient("malformed ServerInfoRequest — client too old?")`; version mismatch → `RejectClient` with both versions; else encode + send `ServerInfo` (sampleRate, version).
  - `default`: unconfigured sender → `RejectClient("unrecognized message type %u; server protocol %u")` (pre-phase-1 clients get a clear reason instead of a 30 s hang); else log and ignore.

## Step 4 — Client (`VoiceChatClient/`) — builds at end of step

- **`Config.h`/`Config.cpp`**: `ClientConfig` gains `std::string name;`. Add `("n,name", "Display name shown to other clients (up to 31 characters)", ...->default_value(""))`; validate length ≤ `MaxClientNameLength` per the existing cerr-and-return-1 pattern (include `../Common/Messages/MessageTypes.h`).
- **`SocketClient.cpp`**:
  - `ClosedByPeer/ProblemDetectedLocally` (~line 90): add `isConnected = false;` (today only `connection` is invalidated, so main's wait loops can't see a rejection).
  - Receive switch — every case decodes via `ReadStream`; decode failure → drop + log:
    - `AUDIO`: decode header + samples into a local/static `AudioData`, existing playback loop; range checks now live in `serialize_sample_array`. No senderId printing in the audio path (latency; phase 3 consumes it).
    - `SERVER_INFO`: decode (fixes the pre-existing unchecked read).
    - `ROSTER_LIST` / `CLIENT_JOINED` / `CLIENT_CONFIG_CHANGED` / `CLIENT_LEFT`: decode, then plain debug printfs — roster prints `"roster: %u clients online"` + one line per entry (id, name, channel, muted); join/changed/left print id/name details. (`serialize_name` already forces termination.)
  - `RequestServerInfo()`: encode via `WriteStream` instead of raw struct send.
- **`AudioTools.cpp`** send path: encode the staged `AudioData` into a stack buffer (`MaxWireSize` 2057 bytes), send `BytesWritten()`. `VOICECHAT_AUDIO_STATS` hook here.
- **`main.cpp`**:
  - `HasServerInfo` wait loop (lines 61–69): also bail if `!IsConnected()` — how a version-rejected client exits promptly (the callback already printed the server's reason).
  - After sample-rate validation: check `GetServerInfo().protocolVersion == PROTOCOL_VERSION`, print both and exit on mismatch.
  - Config send (~line 93): fill muted = 0 and name from `config.name`, encode + send.

## Step 5 — Verification

Build each project after its step (the endian guard and template instantiation checks run at compile time):
```
cmd /c "call \"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" && cmake --build <VoiceChatServer|VoiceChatClient>\out\build\x64-Release"
```

Manual smoke test (extends the phase-0 flow):
1. Start server; client A `--name alice --loopback` → roster prints `roster: 1 clients online`, loopback voice audible (proves encode → stamp → decode round-trip on the audio path).
2. Client B `--name bob` → A prints `client joined: bob (...)`; B's roster lists both; two-way voice works.
3. Ctrl+C B → A prints `client left: id 2`; server logs the leave.
4. Client with no `--name` → roster shows `client-<id>`.
5. Negative: scratch client built with `PROTOCOL_VERSION = 2` → prompt disconnect, server's reason string printed client-side, no 30 s hang.
6. Optional: rebuild client with `-DVOICECHAT_AUDIO_STATS`, talk for a minute, confirm the histogram prints on exit.

## Step 6 — Roadmap docs

- `docs/roadmap/README.md`: phase 1 status → done (with date). **Update the "Decisions already made" section**: replace "Wire format stays raw POD structs" with the serialization decision (unified bit-stream serializer, explicit little-endian, big-endian unsupported, compression seams reserved but unimplemented).
- `docs/roadmap/phase-1-protocol-presence.md` "Notes for later phases": final message type values, the wire-format table from Step 2, `PROTOCOL_VERSION = 1`, `MaxClientNameLength = 31`, values 2/3 reserved, event scope (broadcast to all configured; clients filter by channel), senderId server-stamped at wire bytes 1–4, the serialize_sample_array seam + stats hook for the phase-2/3 compression decision.

## Knowingly out of scope

- No compression of any kind (bit core + helpers are the seam; measure first).
- No `--muted` CLI flag (wire carries it; nothing sets it until phase 3).
- No name uniqueness, rate limiting, auth, big-endian support.
- The overflow-prone `uint16` byte counters in `Server.h` stay as-is.
