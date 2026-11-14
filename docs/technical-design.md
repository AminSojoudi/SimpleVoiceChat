# Technical design overview

State as of 2026-11-13 (after roadmap phase 2). For planned changes see [roadmap/README.md](roadmap/README.md). For exact wire layouts see "Notes for later phases" in [roadmap/phase-1-protocol-presence.md](roadmap/phase-1-protocol-presence.md).

## System at a glance

```
 VoiceChatClient                       VoiceChatServer                     VoiceChatClient
 ┌───────────────────────┐   UDP/GNS   ┌──────────────────────────┐   UDP/GNS  ┌─────────────────┐
 │ mic → RtAudio cb ─────┼── AUDIO ───▶│ stamp senderId, relay to │──AUDIO────▶│ sender rings →  │
 │ speakers ◀ mix rings  │◀── AUDIO ───│ same-channel clients     │            │ RtAudio cb → 🔊 │
 │ Update(): poll net    │◀─ events ───│ client list + events     │──events──▶ │                 │
 └───────────────────────┘             └──────────────────────────┘            └─────────────────┘
```

- **Server** relays audio. It never mixes or decodes samples. It owns the sample rate and the client list.
- **Client** captures mono 16-bit PCM, sends it, and plays what it receives, mixing all senders (mono copied to stereo).
- **Common** holds the wire protocol, shared as headers by both executables.

## Transport

- Valve GameNetworkingSockets over UDP. Default port 27020.
- Every connection has two GNS lanes. Lane 0 carries `AUDIO`, sent `k_nSteamNetworkingSend_UnreliableNoNagle`: a lost packet is gone and does not delay the ones after it. Lane 1 carries everything else, sent `k_nSteamNetworkingSend_ReliableNoNagle`. Each side configures lanes for what it sends (`ConfigureLanes`), and `SendToConnection` picks lane and reliability from the type byte (`Common/Messages/MessageTypes.h`).
- Audio messages carry a per-sender `sequence` so receivers can drop late or duplicate packets.
- GNS encrypts and authenticates the connection. There is no user auth.

## Connection flow

1. Client connects. Server accepts and assigns a client id (`uint32`, counts up from 1). It adds an entry to `clients` (`std::map<HSteamNetConnection, ClientInfo>`), the server's single source of truth.
2. Client sends `GET_SERVER_INFO` with its `protocolVersion`. Server answers with `SERVER_INFO` (sample rate and server protocol version).
3. Client checks the version and sample rate. It sizes its audio buffer as `sampleRate * syncIntervalMs / 1000` frames.
4. Client sends `SET_CLIENT_CONFIG` (channel, loopback, muted, name). On the first accepted config, the server sends this client a `CLIENT_LIST` snapshot and broadcasts `CLIENT_JOINED`. Later configs broadcast `CLIENT_CONFIG_CHANGED`.
5. Audio flows. The server drops audio from clients that have no config yet.
6. On disconnect, the server broadcasts `CLIENT_LEFT` and erases the entry.

A protocol mismatch in step 2 or 4 makes the server close the connection with a reason string, such as "server protocol X, client protocol Y". Core fires `DISCONNECTED` with it as the reason, and the CLI prints it and exits.

Events go to all configured clients on every channel. Each entry carries its channel, and clients filter locally.

## Wire protocol

- Every message has one `template<typename Stream> bool Serialize(Stream&)`. The same field list writes (`WriteStream`), reads (`ReadStream`), and measures (`MeasureStream`) (`Common/Serialization/Streams.h`).
- Streams wrap a bit-level writer and reader (`BitStream.h`). Every field today has a byte-multiple width, so the bytes on the wire are plain little-endian.
- Every count is range-checked on read. A failed read drops the message with a log line.
- Byte 0 is the message type. Ranges: requests 1–63, events 64–127, data 128+ (`Common/Messages/MessageTypes.h`).
- `PROTOCOL_VERSION` is checked for strict equality. Bump it on any wire change.
- `serialize_sample_array` (`Serialize.h`) is the one place that encodes audio samples. It is the hook for adding compression later. Nothing is compressed today.

## Server

- Files: `VoiceChatServer/main.cpp`, `Server.{h,cpp}`, `Config.{h,cpp}`.
- One thread. Every 30 ms it polls incoming messages (poll group) and runs GNS callbacks. SIGINT and SIGTERM stop the loop.
- Audio relay: check the size against `count` (bytes 9–12) without a full decode. Overwrite `senderId` (bytes 1–4) in the received buffer once, then send the same buffer to each configured client on the sender's channel. The sender gets its own audio back only with `loopback` set. The relay scans every client in the map.
- Flags: `--port`, `--bind`, `--log-level`, `--sample-rate` (default 44100).
- `Server` keeps all state in static members.

## Client

- `VoiceChatCore/` is a static library that does all client work: connection, handshake, client list, audio, talking detection. Public header `VoiceChatSession.h`. It never prints; everything goes out as events.
- `VoiceChatClient/` is a thin CLI over Core (`main.cpp`, `Config.{h,cpp}`). It prints events, reads commands from stdin (`m` mute, `l` loopback, `c <n>` channel, `q` quit), and exits on `q`, Ctrl-C, or a dropped connection (exit code 1).
- `VoiceChatSession` is instance-based, but only one can exist per process: GNS callbacks carry no user pointer, so they find the session through a file-static pointer.
- Threads:
  - **Caller** (calls `Update` every `syncIntervalMs`): runs GNS callbacks, receives and decodes messages, drops late or duplicate audio by `sequence`, pushes samples into per-sender rings, keeps the client list and talking state, sends config, fires every event.
  - **RtAudio callback** (realtime): measures the mic peak, captures and sends audio while the silence gate is open and not muted, mixes every sender ring into the output, and plays silence for missing samples. It logs nothing itself, but sending can block for a moment: GNS `SendMessages` takes an internal lock and allocates, and GNS may queue a debug log. If that causes overflows, move sends to `Update` through a second ring.
  - **GNS internal** and the **RtAudio error callback**: queue log lines under a mutex. `Update` fires them as LOG events.
- Playback: up to 8 senders, each with its own lock-free SPSC ring (`SampleRing.h`) of a few audio buffers, never under 0.1 s. A full ring drops the newest samples. A gap in `sequence` plays up to two packets of silence. A sender silent for 2 s, a sender that leaves, or a channel change frees the slot.
- Silence gate: a client sends audio only while its mic peak is above 0.02 (about −34 dBFS, `TalkingLevel`) or was within the last 200 ms. A quiet room sends nothing. `sequence` counts sends only, so a pause is not a gap.
- Talking: a peer talks while its audio arrives, with a 200 ms hold. My own talking is the silence gate. Audio from an id that isn't in the client list is dropped.
- CLI flags: `--address`, `--port`, `--channel`, `--name`, `--loopback`, `--log-level` (`error|warning|info|debug`), `--sync-interval`.

## Build and deploy

- One root CMake project adds `VoiceChatServer/`, `VoiceChatCore/`, and `VoiceChatClient/`. C++17. The `Common/` headers are listed in the server and Core. Core and the CLI build only with `VOICECHAT_BUILD_CLIENT`.
- One vcpkg manifest at the root. Shared libraries sit at the top level; program-only libraries sit under the `server` or `client` feature. `VOICECHAT_BUILD_SERVER` / `VOICECHAT_BUILD_CLIENT` pick which programs (and features) build.
- `Dockerfile` builds only the server (`VOICECHAT_BUILD_CLIENT=OFF`) on Ubuntu 22.04 and ships a slim runtime image.
- CI builds both programs from the root, on Windows and Ubuntu, then runs `SampleRingTest` (assert-based, the only automated test).

## Known limits

- One `VoiceChatSession` per process (GNS callbacks carry no user pointer).
- No jitter buffer. Gaps are filled with silence sized from the next packet. At most 8 senders play at once; more are dropped with a log line.
- `AUDIO_SAMPLE` is `uint16`, but the stream is signed 16-bit. Level math must cast to `int16_t`.
- The server's `CLIENT_LIST` holds at most 64 entries.
