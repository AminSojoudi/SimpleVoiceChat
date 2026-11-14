# Phase 2: Extract VoiceChatCore client library

## Goal

A new `VoiceChatCore/` static library target that owns everything a voice chat client does except UI: connect/handshake, config, audio capture/playback, client list state, mute. It exposes an instance-based, callback-driven C++ API and never touches stdout. `VoiceChatClient` (the CLI) becomes a thin wrapper over Core and keeps working. Phase 3's terminal client builds on this API; phase 4 wraps it in a C ABI for Unity.

## Prerequisite

Phase 1 (protocol + presence) is done — Core's API should expose client list/talking events, which need the phase 1 wire messages.

## Why

The current client code cannot be embedded in a TUI or game engine:

- **Everything is static.** All of `SocketClient`'s state is static members (`SocketClient.h`) — a process-global singleton. The GameNetworkingSockets status callback is a raw function pointer with no user-data channel, which is why it ended up static.
- **It prints from three threads.** Raw `printf`/`std::cout` sites are spread across `SocketClient.cpp` (~13 sites), `AudioTools.cpp`, `Utils.h`, and even `AudioMessage.h::AddInput`. The GNS debug callback prints from a background thread; the RtAudio callback prints from the realtime audio thread. A TUI owns the whole screen — all of this must go through a callback/sink.
- **Partial shutdown path.** Since phase 1, `main.cpp` handles SIGINT, calls `StopRecording`, and the `SocketClient` dtor closes the connection. `GameNetworkingSockets_Kill()` still never runs.

## Current state inventory (verified 2026-08-04 — key facts for design)

### Structure
- `SocketClient` (VoiceChatClient/SocketClient.{h,cpp}): all-static class. Connect, per-message dispatch switch (AUDIO → per-sample push into `NetworkBuffer`; SERVER_INFO → static `serverInfo`), `Send` used for both config and audio, `PollConnectionStateChanges` = `RunCallbacks()` on the calling thread.
- `AudioTools` (VoiceChatClient/AudioTools.{h,cpp}): one duplex RtAudio stream (mono capture in, stereo playback out, `RTAUDIO_SINT16`, `RTAUDIO_HOG_DEVICE | RTAUDIO_SCHEDULE_REALTIME`). The RtAudio callback is a free function using file-scope globals (`AudioData data`, raw `NetworkBuffer*`, raw `SocketClient*`). It captures mic → `AudioData`, drains `NetworkBuffer` → speakers, and calls `SocketClient::Send` directly from the audio thread.
- `NetworkBuffer` (VoiceChatClient/Utils.h) wraps `ConcurrentBag<uint16>` (VoiceChatClient/ConcurrentBag.hpp): mutex-per-operation vector, locked once per sample on both producer and consumer sides, O(n) erase-from-front. Works, but wrong primitive for realtime audio — replace with a lock-free SPSC ring buffer in Core.
- `main.cpp` flow: parse config → connect → wait connected (30s deadline) → request server info → wait for it → validate sample rate and derive `bufferFrames = sampleRate * syncIntervalMs / 1000` → send `SetClientConfig` → `StartRecording` → infinite poll loop. This whole sequence is the state machine Core should own.

### Threads (3) and races to fix
- T1 main: polls network, runs GNS callbacks, writes `NetworkBuffer`.
- T2 RtAudio callback: reads `NetworkBuffer`, sends audio via GNS, realtime priority.
- T3 GNS internal: calls the debug-output function.
- Known races: `isConnected` (plain bool, written T1, read T2), `connection` handle (T1 can invalidate while T2's `Send` reads it). Fix with atomics in Core.

### Known defects to fix during extraction (not before)
- Playback underrun leaves the output buffer untouched (stale garbage instead of silence) — `AudioTools.cpp` `record()`, missing `else` that should zero the output.
- ~~Disconnect never resets `isConnected`~~ — fixed in phase 1 (close callback sets it false). Core still needs a real disconnected state + event.
- `SocketClient` dtor closes the connection (phase 1) but never calls `GameNetworkingSockets_Kill`; Core needs the full clean shutdown.
- ~~SERVER_INFO handler has no size check~~ — fixed in phase 1 (decodes through `ReadStream`).
- `AUDIO_SAMPLE` is `uint16` but the stream format is signed 16-bit (`RTAUDIO_SINT16`). Bit-exact so it works, but any level/VU math (phase 3 needs "am I talking") must cast to `int16_t`. Consider changing the typedef to `int16` — it's wire-compatible.

## Scope

1. **New `VoiceChatCore/` folder + CMake static library target**, containing the session class, audio engine, ring buffer, and the client-side protocol handling. `Common/Messages` stays where it is (server uses it too).
2. **Instance-based `VoiceChatSession` (name open)** exposing roughly: `Connect(config)`, `Disconnect()`, `SetMuted(bool)`, `Update()` (pump network + events on caller's thread), state queries (connection state, client list snapshot, per-client talking state), and a callback/observer interface: connected, disconnected(reason), client list changed, peer joined/left, peer talking started/stopped, log(level, message).
   - GNS's status callback has no user pointer — route it through a file-static registry mapping connection → instance (or a documented single-instance assumption; decide during planning).
   - Callbacks fire on the thread that calls `Update()` (main/UI thread), never on the audio or GNS threads. Internal events from other threads get queued.
3. **Logging sink** replacing every printf across the moved code, including the GNS debug callback (T3) and audio-thread sites (T2) — those must enqueue, not call the sink inline.
4. **Mute** implemented in the audio callback (skip `AddInput` of mic samples when muted; `std::atomic<bool>`), visible state sent to the server via the phase 1 config message.
   - **Runtime options (added 2026-11-12).** Mute, loopback, and channel can all change mid-session. The protocol already supports it: the client resends `SetClientConfig`, the server applies it and broadcasts `CLIENT_CONFIG_CHANGED`. No wire change. Core keeps the current options and resends the full config on any change. The phase 3 options panel (see phase-3 doc) is built on this.
   - **API shape — decision for planning, ask the user.** Options: separate setters (`SetMuted`, `SetLoopback`, `SetChannel`), or one call that takes an options struct (`UpdateOptions(opts)`). Recommendation: the options struct. One code path sends the config, and a new option later is one more field instead of one more method. The phase 4 C API needs a flat setter per field either way.
   - Changing channel affects local state: the client list view filters by channel, and the playback buffer should be cleared so audio from the old channel doesn't keep playing.
5. **Talking detection**: local mic energy for "am I talking" (cast samples to `int16_t`), and per-peer "is talking" derived from received audio sender ids (phase 1) with a ~200ms hold-off.
6. **Clean shutdown path**: stop stream, close connection, kill GNS; make the CLI handle Ctrl-C (SIGINT → quit flag) as proof.
7. **Rewrite `VoiceChatClient/main.cpp`** as a thin consumer of Core (connect, print log lines, Ctrl-C to quit). Delete the now-moved files from the CLI target.
8. **Replace `ConcurrentBag` buffer with an SPSC ring buffer** for playback samples (single producer = network thread, single consumer = audio thread).
9. **Play silence on underrun.** When the ring buffer has fewer samples than the callback needs, zero the output buffer. Today `record()` leaves it untouched and stale audio plays again (still open, verified 2026-11-12).
10. **Unreliable audio — design decision for planning, ask the user.** Client and server send everything with `k_nSteamNetworkingSend_ReliableNoNagle` (`SocketClient::Send`, `Server::SendMessage`). For audio this means one lost packet stalls every packet behind it until the resend arrives, so latency grows under loss. Options:
    - **Keep reliable.** No change. Fine on LAN, bad on lossy links.
    - **Unreliable audio, reliable everything else.** Audio sends (client upstream and server relay) use `k_nSteamNetworkingSend_UnreliableNoNagle`; requests and events stay reliable. Unreliable messages can be dropped or arrive out of order, so playback needs a per-sender sequence number to drop late packets and fill gaps with silence. That is a wire change: add the field to `AudioData`, bump `PROTOCOL_VERSION`, and keep the server's fixed byte offsets in the `AUDIO` relay case in sync.
    - **GNS connection lanes** (since GNS 1.4, `ConfigureConnectionLanes`): split one connection into lanes with their own priority and weight, so audio and control messages don't block each other. Lanes don't remove head-of-line blocking for reliable audio. They do help keep events and config from being stuck behind audio. Look at them alongside the unreliable option.
    - Fits this phase because Core owns the playback buffer, which is where loss handling lives. The server relay change lands in the same commit.

## Out of scope

- Any TUI code (phase 3) — but the Core API surface is designed against phase 3's feature list, so review that doc when designing the API.
- C ABI / `extern "C"` (phase 4). Keep the callback interface flat and simple so the C wrapper stays mechanical.
- Opus/compression, jitter buffering beyond the existing simple buffer — worthwhile, but separate work.

## Acceptance criteria

- `VoiceChatCore` builds as a library; CLI client links it and voice chat works end to end on localhost as before (smoke test: server + 2 clients, or 1 client with `--loopback`).
- Zero direct stdout/stderr writes anywhere in Core (grep for `printf`, `cout`, `cerr`); the CLI prints only via the log callback.
- Ctrl-C on the CLI exits cleanly: stream stopped, connection closed (server logs a clean disconnect, not a timeout).
- Kill the server while a client runs: client gets a disconnected event and exits instead of spinning.
- Mute toggle demonstrable (even if just a debug key/flag in the CLI): peer stops receiving audio while muted, resumes after.
- Loopback and channel change mid-session through Core's options API: the server log shows the config change, and other clients get `CLIENT_CONFIG_CHANGED`.

## What was done

Done 2026-11-13 in three chunks, following [phase-2-implementation-plan.md](phase-2-implementation-plan.md). The CLI is now a ~100-line consumer of Core. Headless smoke test passed (handshake, client list, mute, loopback, channel change, clean close, server kill). Nobody has listened to the audio yet (mixing, two voices at once, no old-channel audio after a channel change); that check is still open.

Deviations from the plan:
- The old client set `RTAUDIO_HOG_DEVICE | RTAUDIO_SCHEDULE_REALTIME` but never passed the options to `openStream`. Core drops the flags instead of turning them on.
- A channel change frees the playback slots but keeps the sender map, so each sender's talking state still ends through the normal hold timer.
- `Disconnect` closes the connection with linger. That is enough for the server to log a clean close; no extra wait before `GameNetworkingSockets_Kill`.
- `LogLevel` values are `Error`, `Warning`, `Info`, `Debug`. `windows.h` defines `ERROR` as a macro.
- Core's `Connect` rejects a name longer than 31 characters with a LOG event. The CLI no longer checks it, so it doesn't need the `Common/` headers.
- The CLI `--log-level` takes `error|warning|info|debug`. `none`, `verbose`, and `everything` are gone.
- Review fixes (2026-11-14):
  - Silence gate on the sender. The audio callback sends only while the mic peak is above `TalkingLevel` or was within the 200 ms hold (counted in audio buffers, sized at `StartAudio`). A quiet room sends nothing, so peers no longer show as talking all session. `sequence` counts sends only, so a pause leaves no gap and receivers didn't change. My own talking state comes from the same gate. No wire change.
  - Audio from an id that isn't in the peer list is dropped. Late audio after `CLIENT_LEFT` no longer brings a peer back.
  - `CLIENT_LIST`, `CLIENT_JOINED`, and `CLIENT_CONFIG_CHANGED` build peers with the talking state the session already has.
  - A freed playback slot is marked reusable only after the audio thread has emptied its ring.

## Notes for later phases

- Header: `VoiceChatCore/VoiceChatSession.h`. Link the `VoiceChatCore` target. It needs no GNS, RtAudio, or `Common/` headers.
- Public surface:
  - `VoiceChatSession(std::function<void(const Event&)> onEvent)`, destructor disconnects and kills GNS.
  - `bool Connect(const VoiceChatConfig&)`, `void Disconnect()` (no event), `void Update()`, `void UpdateOptions(const VoiceChatOptions&)`.
  - `State()`, `Options()`, `Peers()` (all channels, includes me), `SelfId()` (0 until connected), `MicLevel()` (0..1).
  - `VoiceChatConfig`: `address`, `port`, `name`, `syncIntervalMs`, `logLevel`, `options`. `VoiceChatOptions`: `channel`, `muted`, `loopback`.
  - `EventType`: `CONNECTED`, `DISCONNECTED` (text = reason), `PEER_LIST`, `PEER_JOINED`, `PEER_LEFT`, `PEER_CHANGED`, `TALKING_CHANGED` (peer id == `SelfId()` means my mic), `LOG`.
- Call `Update` every `syncIntervalMs`. It drives playback, talking timers, and the 30 s handshake timeout.
- One session per process. The constructor asserts if a second one exists (GNS callbacks carry no user pointer).
- Events fire inside `Update`, on the caller's thread. A failed `Connect` also fires LOG before it returns. Don't call `Connect` or `Disconnect` from the event callback.
- `Peer` has no `loopback` field, so a loopback change shows up as `PEER_CHANGED` with nothing visibly different.
- Senders stop sending in silence (mic below `TalkingLevel` for 200 ms). `TalkingLevel` in `VoiceChatSession.cpp` is the one calibration knob: too low and a noisy mic sends all the time, too high and quiet speech gets cut. Speech onset is not buffered, so the first buffer above the level is the first one sent.
- Fixed: audio that arrives before the client list used to fire `TALKING_CHANGED` with an empty name. Audio from unknown ids is now dropped, and the list carries the current talking state.
- Left as is (cosmetic): audio rides lane 0 and config events lane 1, so a peer's talking event can arrive before the `PEER_CHANGED` that unmuted them.
