# Roadmap: from CLI voice chat to multi-client platform

## Where this comes from

The project today has three parts: `VoiceChatServer`, `VoiceChatClient` (a plain CLI client), and `Common/Messages` (the wire protocol, raw POD structs). The goal is a richer terminal client (client list, talking indicators, mute) and, later, other clients such as a Unity plugin — all sharing one core client library.

The work is split into phases. Each phase has its own doc in this folder with enough context to plan and implement it in a fresh session. Do the phases in order — each one builds on the previous.

## Phases

| Phase | Doc | Status |
|---|---|---|
| 1. Protocol restructure + server presence | [phase-1-protocol-presence.md](phase-1-protocol-presence.md) | done (2026-08-04) |
| 2. Extract VoiceChatCore client library | [phase-2-core-library.md](phase-2-core-library.md) | not started |
| 3. Terminal UI client | [phase-3-terminal-client.md](phase-3-terminal-client.md) | not started |
| 4. C API + Unity plugin (future) | [phase-4-c-api-unity.md](phase-4-c-api-unity.md) | not started |

Update the status column when a phase is done, and note anything a later phase needs to know that changed during implementation.

## Decisions already made (apply to all phases)

- **Monorepo.** All clients, the server, and shared code stay in this repository. Protocol changes land atomically with the server and client changes that use them.
- **Wire format: explicit field-by-field serialization** (decided during phase 1, supersedes the earlier "raw POD structs" rule). Every message has one `template<typename Stream> bool Serialize(Stream&)` field list that serves write, read, and measure (`Common/Serialization/`). Streams wrap a bit-level writer/reader; today every field is a byte-multiple width, so the wire is plain little-endian bytes. Big-endian hosts are unsupported (compile-time `#error`). The bit core and `serialize_sample_array` are the reserved seams for future audio compression — none implemented yet. The first wire byte stays a `uint8_t type` discriminator. There are no legacy clients to stay compatible with; the protocol can change freely until phase 4 (bump `PROTOCOL_VERSION` on any wire change).
- **Message types get split into categories** (requests, events, data) instead of one flat enum. Phase 1 defines this.
- **Core library before TUI.** The terminal client is a thin UI over `VoiceChatCore`. Core is callback-driven, instance-based (no static singletons), and never prints to the terminal.
- **No C ABI until phase 4.** Core stays plain C++ until Unity is actually being built; the C wrapper is a thin layer added later.
- **TUI library: FTXUI** (available via vcpkg, cross-platform, fits the animation/re-render needs).

## Build notes (every phase)

- Windows, Visual Studio 2022, vcpkg for dependencies (GameNetworkingSockets, RtAudio, cxxopts).
- Every build shell command must import the VS dev environment first:
  `cmd /c "call \"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" && cmake --build <build-dir>"`
- Existing build trees: `VoiceChatServer\out\build\x64-Release` and `VoiceChatClient\out\build\x64-Release` (configured via Visual Studio's CMakeSettings workflow).
