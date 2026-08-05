# Phase 2: Extract VoiceChatCore client library

## Goal

A new `VoiceChatCore/` static library target that owns everything a voice chat client does except UI: connect/handshake, config, audio capture/playback, client list state, mute. It exposes an instance-based, callback-driven C++ API and never touches stdout. `VoiceChatClient` (the CLI) becomes a thin wrapper over Core and keeps working. Phase 3's terminal client builds on this API; phase 4 wraps it in a C ABI for Unity.

## Prerequisite

Phase 1 (protocol + presence) is done — Core's API should expose client list/talking events, which need the phase 1 wire messages.

## Why

The current client code cannot be embedded in a TUI or game engine:

- **Everything is static.** All of `SocketClient`'s state is static members (`SocketClient.h`) — a process-global singleton. The GameNetworkingSockets status callback is a raw function pointer with no user-data channel, which is why it ended up static.
- **It prints from three threads.** Raw `printf`/`std::cout` sites are spread across `SocketClient.cpp` (~13 sites), `AudioTools.cpp`, `Utils.h`, and even `AudioMessage.h::AddInput`. The GNS debug callback prints from a background thread; the RtAudio callback prints from the realtime audio thread. A TUI owns the whole screen — all of this must go through a callback/sink.
- **No shutdown path.** `main.cpp` has no signal handling; the main loop is infinite, `StopRecording` is never called, connections are never closed, `GameNetworkingSockets_Kill()` never runs.

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
- Disconnect never resets `isConnected`, so the main loop spins printing "connection is invalid" every 5 ms after a drop. Core needs a real disconnected state + event.
- `SocketClient` dtor closes nothing; add clean shutdown (close connection, stop stream, `GameNetworkingSockets_Kill`).
- SERVER_INFO handler copies the struct with no size check (unlike AUDIO which validates) — add the check.
- `AUDIO_SAMPLE` is `uint16` but the stream format is signed 16-bit (`RTAUDIO_SINT16`). Bit-exact so it works, but any level/VU math (phase 3 needs "am I talking") must cast to `int16_t`. Consider changing the typedef to `int16` — it's wire-compatible.

## Scope

1. **New `VoiceChatCore/` folder + CMake static library target**, containing the session class, audio engine, ring buffer, and the client-side protocol handling. `Common/Messages` stays where it is (server uses it too).
2. **Instance-based `VoiceChatSession` (name open)** exposing roughly: `Connect(config)`, `Disconnect()`, `SetMuted(bool)`, `Update()` (pump network + events on caller's thread), state queries (connection state, client list snapshot, per-client talking state), and a callback/observer interface: connected, disconnected(reason), client list changed, peer joined/left, peer talking started/stopped, log(level, message).
   - GNS's status callback has no user pointer — route it through a file-static registry mapping connection → instance (or a documented single-instance assumption; decide during planning).
   - Callbacks fire on the thread that calls `Update()` (main/UI thread), never on the audio or GNS threads. Internal events from other threads get queued.
3. **Logging sink** replacing every printf across the moved code, including the GNS debug callback (T3) and audio-thread sites (T2) — those must enqueue, not call the sink inline.
4. **Mute** implemented in the audio callback (skip `AddInput` of mic samples when muted; `std::atomic<bool>`), visible state sent to the server via the phase 1 config message.
5. **Talking detection**: local mic energy for "am I talking" (cast samples to `int16_t`), and per-peer "is talking" derived from received audio sender ids (phase 1) with a ~200ms hold-off.
6. **Clean shutdown path**: stop stream, close connection, kill GNS; make the CLI handle Ctrl-C (SIGINT → quit flag) as proof.
7. **Rewrite `VoiceChatClient/main.cpp`** as a thin consumer of Core (connect, print log lines, Ctrl-C to quit). Delete the now-moved files from the CLI target.
8. **Replace `ConcurrentBag` buffer with an SPSC ring buffer** for playback samples (single producer = network thread, single consumer = audio thread).

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

## Notes for later phases

- After this phase, record the final Core public API (header path + class surface) here for phase 3 planning:
  - (fill in when done)
