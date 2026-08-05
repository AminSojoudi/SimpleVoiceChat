# Phase 1: Protocol restructure + server presence

Status: done 2026-08-04. Implemented per [phase-1-implementation-plan.md](phase-1-implementation-plan.md); final wire facts are in "Notes for later phases" at the bottom.

## Goal

Give the protocol and server everything the future terminal client needs to show a roster, online counts, and "who is talking" — and reorganize message types into categories while we're changing the protocol anyway. After this phase, the plain CLI client still works, and the server exposes all presence information on the wire.

## Why

The terminal client (phase 3) needs features the protocol cannot express today:

- **Roster / online count**: the server tracks `ClientInfo` per connection (`VoiceChatServer/Server.h`) but never tells clients about each other.
- **Who is talking**: relayed `AudioData` carries no sender identity. A client receiving audio cannot tell who sent it. With several people talking at once this is unfixable client-side.
- **Names**: clients have no name; the server only knows endpoints.
- **Message organization**: all message types sit in one flat enum (`Common/Messages/MessageTypes.h`). As we add roster events this becomes a mess. Split types into categories: **requests** (client → server), **events** (server → client), **data** (both directions, currently just audio).

## Current state (verified 2026-08-04)

- Wire format: raw POD structs, first byte is `uint8_t type`. No packing pragma — client and server must be the same ABI (fine for now).
- `Common/Messages/MessageTypes.h`: flat enum `SET_CLIENT_CONFIG = 1, AUDIO = 2, GET_SERVER_INFO = 3, SERVER_INFO = 4`.
- `SetClientConfig` (`Common/Messages/ClientConfigMessage.h`): `type`, `int64 channel`, `uint8_t loopback`. Built to grow — this is where `name` and `muted` go.
- Server: single-threaded, `std::map<HSteamNetConnection, ClientInfo> clients` in `VoiceChatServer/Server.h` is the single source of truth. `ClientInfo` holds `channel`, `loopback`, `hasConfig`, `endpoint`. Entries are created at connection accept, erased on disconnect. Audio relay is a linear scan over `clients` filtered by channel (`Server.cpp`, `AUDIO` case).
- `AudioData` (`Common/Messages/AudioMessage.h`): variable-length wire message with `WireSize()` / `HeaderSize()` helpers and length validation on both sides. `HeaderSize()` uses `offsetof` — note there are 3 padding bytes after `type` on MSVC x64.

## Scope

1. **Message categories.** Reorganize `MessageTypes.h` into request/event/data categories with distinct numeric ranges so a received type byte maps to its category (for example requests 1–63, events 64–127, data 128+). Keep one `uint8_t` discriminator on the wire. Exact shape (one enum with ranges vs. separate enums) is a design decision for phase planning.
2. **Client identity.** Server assigns each connection a small numeric client id. Add a `name` field to `SetClientConfig` (fixed-size char array — wire structs are POD, no std::string).
3. **Roster events (server → client):**
   - Full client list sent to a client after its config is accepted (ids, names, channels or at least same-channel members, muted flags, online count).
   - Join / leave / config-changed events pushed to affected clients so rosters stay current without polling.
4. **Sender id on audio.** Stamp the sender's client id into the audio message so receivers know who is talking. Decide during planning: one shared `AudioData` struct with a `senderId` field the client leaves empty and the server fills before relay, or separate upstream/downstream audio structs. Watch out: the server currently relays the incoming buffer bytes as-is; stamping an id means the relay path modifies (or rebuilds the header of) the message.
5. **Muted as visible state.** Add `muted` to `SetClientConfig` and `ClientInfo` and include it in roster data. (Actually silencing the mic is client-side, later phase.)
6. **Protocol version handshake.** Add a single `PROTOCOL_VERSION` constant to `Common/Messages`, bumped on any incompatible wire change. The client sends it in the first message the server handles (hello or `SetClientConfig`); the server rejects mismatches with a clear reason string ("server protocol X, client protocol Y") instead of silently misreading structs. Include the server's version in `ServerInfo` too so the client can report mismatches from its side. Strict equality check — no compat ranges or per-message versioning while all clients ship from this repo. Also add `static_assert(sizeof(...))` checks on wire structs (layout currently depends on MSVC padding; a packing change later is itself a version bump).
7. **Keep the CLI client working.** Update `VoiceChatClient` for the changed structs. It can ignore roster events (default switch case) — it does not need new features.

## Out of scope

- Any client-side UI for the roster (phase 3).
- Client-side talking detection / VU meters (phase 3, needs signed-sample handling — see phase 2 notes on the uint16/int16 mismatch).
- Core library extraction (phase 2).
- Rate limiting, name uniqueness enforcement, auth — not needed yet.

## Acceptance criteria

- Server builds and relays audio with sender id stamped; existing 2-client voice flow still works (manual localhost test as in the phase-0 smoke test: start server, connect clients, watch server log).
- A client connecting with a name appears in roster events sent to other clients on the same channel (verifiable with a debug printf in the CLI client's message switch).
- Join and leave both produce events observable by another connected client.
- Undersized versions of every new message are dropped with a log line (mirror the existing `SetClientConfig` size check in `Server.cpp`).

## Notes for later phases

Final state after implementation (2026-08-04):

- **Serialization, not POD casting.** Every message serializes field by field through `Common/Serialization/` (`BitStream.h`, `Streams.h`, `Serialize.h`). Wire format is explicit little-endian; big-endian hosts fail the build. In-memory structs are plain (no packing pragmas, no wire-sizeof asserts) — the serializer alone defines the wire layout.
- `PROTOCOL_VERSION = 1` (`Common/Messages/MessageTypes.h`). Bump on ANY wire change, including field encodings. Strict equality; the server rejects mismatches through the connection close reason string, which the client callback prints.
- `MaxClientNameLength = 31`; names are 32 bytes fixed on the wire, termination always forced on read. Empty name → server substitutes `client-<id>`.
- **Message type values** (one byte, first on the wire; range = category — requests 1–63, events 64–127, data 128+; values 2 and 3 stay reserved so pre-versioning clients never alias a current message):
  - `SET_CLIENT_CONFIG = 1`, `GET_SERVER_INFO = 4`
  - `SERVER_INFO = 64`, `CLIENT_LIST = 65`, `CLIENT_JOINED = 66`, `CLIENT_LEFT = 67`, `CLIENT_CONFIG_CHANGED = 68`
  - `AUDIO = 128`
- **Wire layouts** (exact bytes, no padding):

  | Message | Fields in serialize order | Wire bytes |
  |---|---|---|
  | `SetClientConfig` | type u8, protocolVersion u32, channel i64, loopback u8, muted u8, name 32B | 47 |
  | `ServerInfoRequest` | type u8, protocolVersion u32 | 5 |
  | `ServerInfo` | type u8, sampleRate u32, protocolVersion u32 | 9 |
  | `AudioData` | type u8, senderId u32, count u32 (range ≤ 1024), samples 16 bits each | 9 + 2·count |
  | `ClientEntry` (nested) | channel i64, clientId u32, muted u8, name 32B | 45 |
  | `ClientList` | type u8, clientCount u32 (range ≤ 64), entries | 5 + 45·count |
  | `ClientJoined` / `ClientConfigChanged` | type u8, ClientEntry | 46 |
  | `ClientLeft` | type u8, clientId u32 | 5 |

- **Client ids**: `uint32`, server-assigned at accept time, monotonic from 1. 0 = unassigned (what clients send in upstream audio).
- **senderId is server-stamped and unspoofable**: the server overwrites wire bytes 1–4 (LE) of a received audio message in place and relays the same buffer — no per-recipient re-encode. That fixed offset is a wire contract; a serializer change that moves it must update `Server.cpp`'s AUDIO case and bump the version.
- **Event scope**: the full client list snapshot goes to a client when its first config is accepted (`clientCount` doubles as online count, capped at 64 entries). `CLIENT_JOINED` / `CLIENT_LEFT` / `CLIENT_CONFIG_CHANGED` broadcast to ALL configured clients regardless of channel; entries carry the channel and clients filter locally. The messages live in `Common/Messages/ClientListMessages.h`. (Planning docs called this the "roster"; the code name is "client list".)
- **Compression seam for phase 2/3**: the audio sample encoding lives only in `serialize_sample_array` (`Common/Serialization/Serialize.h`). `Common/Serialization/AudioStats.h` (enable with `-DVOICECHAT_AUDIO_STATS`, client send path) records packet sample counts and a bits-required histogram over zig-zag-mapped int16 samples to inform the compression decision. Nothing is compressed yet.
- The CLI client prints client-list/join/leave/config-changed events as plain debug lines (`SocketClient.cpp`); phase 3 replaces those with real UI. The client also exits promptly when the server rejects it (`isConnected` now goes false on close, and the server-info wait loop checks it).
