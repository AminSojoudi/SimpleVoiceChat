# Technical design overview

State as of 2026-11-12 (after roadmap phase 1). For planned changes see [roadmap/README.md](roadmap/README.md). For exact wire layouts see "Notes for later phases" in [roadmap/phase-1-protocol-presence.md](roadmap/phase-1-protocol-presence.md).

## System at a glance

```
 VoiceChatClient                       VoiceChatServer                     VoiceChatClient
 ┌───────────────────────┐   UDP/GNS   ┌──────────────────────────┐   UDP/GNS  ┌─────────────────┐
 │ mic → RtAudio cb ─────┼── AUDIO ───▶│ stamp senderId, relay to │──AUDIO────▶│ NetworkBuffer → │
 │ speakers ◀ NetworkBuf │◀── AUDIO ───│ same-channel clients     │            │ RtAudio cb → 🔊 │
 │ main loop: poll net   │◀─ events ───│ client list + events     │──events──▶ │                 │
 └───────────────────────┘             └──────────────────────────┘            └─────────────────┘
```

- **Server** relays audio. It never mixes or decodes samples. It owns the sample rate and the client list.
- **Client** captures mono 16-bit PCM, sends it, and plays what it receives (mono copied to stereo).
- **Common** holds the wire protocol, shared as headers by both executables.

## Transport

- Valve GameNetworkingSockets over UDP. Default port 27020.
- Both sides send every message with `k_nSteamNetworkingSend_ReliableNoNagle`. Audio is reliable and ordered, so a lost packet delays the packets after it. Moving audio to unreliable sends is the obvious latency fix when it matters.
- GNS encrypts and authenticates the connection. There is no user auth.

## Connection flow

1. Client connects. Server accepts and assigns a client id (`uint32`, counts up from 1). It adds an entry to `clients` (`std::map<HSteamNetConnection, ClientInfo>`), the server's single source of truth.
2. Client sends `GET_SERVER_INFO` with its `protocolVersion`. Server answers with `SERVER_INFO` (sample rate and server protocol version).
3. Client checks the version and sample rate. It sizes its audio buffer as `sampleRate * syncIntervalMs / 1000` frames.
4. Client sends `SET_CLIENT_CONFIG` (channel, loopback, muted, name). On the first accepted config, the server sends this client a `CLIENT_LIST` snapshot and broadcasts `CLIENT_JOINED`. Later configs broadcast `CLIENT_CONFIG_CHANGED`.
5. Audio flows. The server drops audio from clients that have no config yet.
6. On disconnect, the server broadcasts `CLIENT_LEFT` and erases the entry.

A protocol mismatch in step 2 or 4 makes the server close the connection with a reason string, such as "server protocol X, client protocol Y". The client prints it and exits.

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
- Audio relay: check the size against `count` (bytes 5–8) without a full decode. Overwrite `senderId` (bytes 1–4) in the received buffer once, then send the same buffer to each configured client on the sender's channel. The sender gets its own audio back only with `loopback` set. The relay scans every client in the map.
- Flags: `--port`, `--bind`, `--log-level`, `--sample-rate` (default 44100).
- `Server` keeps all state in static members.

## Client

- Files: `VoiceChatClient/main.cpp`, `SocketClient.{h,cpp}`, `AudioTools.{h,cpp}`, `Utils.h` (`NetworkBuffer`), `ConcurrentBag.hpp`, `Config.{h,cpp}`.
- Threads:
  - **Main**: polls the network every `--sync-interval` ms (default 5). It pushes received samples into `NetworkBuffer`.
  - **RtAudio callback** (realtime priority): packs mic samples into `AudioData` and sends them through `SocketClient::Send` from this thread. It also drains `NetworkBuffer` to the speakers.
  - **GNS internal**: calls the debug log function.
- `NetworkBuffer` is a mutex-guarded vector (`ConcurrentBag`), locked once per sample. When full it drops the oldest samples. Its size is a few sync intervals, never under 0.1 s of audio.
- `SocketClient` and the audio callback state are static and global. The client prints straight to stdout from all three threads.
- Flags: `--address`, `--port`, `--channel`, `--name`, `--loopback`, `--log-level`, `--sync-interval`.

## Build and deploy

- One root CMake project adds `VoiceChatServer/` and `VoiceChatClient/`. C++17. The `Common/` headers are listed in both.
- One vcpkg manifest at the root. Shared libraries sit at the top level; program-only libraries sit under the `server` or `client` feature. `VOICECHAT_BUILD_SERVER` / `VOICECHAT_BUILD_CLIENT` pick which programs (and features) build.
- `Dockerfile` builds only the server (`VOICECHAT_BUILD_CLIENT=OFF`) on Ubuntu 22.04 and ships a slim runtime image.
- CI builds both programs from the root, on Windows and Ubuntu. There are no tests.

## Known limits

These are tracked in the roadmap, mostly phase 2:

- Static, process-wide client state. Can't embed or run two sessions. Phase 2 extracts a `VoiceChatCore` library.
- Per-sample mutex in the playback buffer. Phase 2 plans a lock-free SPSC ring buffer.
- On playback underrun the output buffer is not zeroed, so stale audio plays.
- `isConnected` and `connection` are read from the audio thread without atomics.
- `GameNetworkingSockets_Kill()` never runs on client shutdown.
- `AUDIO_SAMPLE` is `uint16`, but the stream is signed 16-bit. Level math must cast to `int16_t`.
- Reliable audio adds latency under packet loss (see Transport). Phase 2 scope item 10 covers the decision.
- `Server.cpp:88` uses the wrong invalid-handle constant. Listed under "Small fixes" in the roadmap README.
- The server's `CLIENT_LIST` holds at most 64 entries.
