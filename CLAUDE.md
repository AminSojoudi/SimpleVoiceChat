# CLAUDE.md

C++17 voice chat over UDP. One server relays 16-bit PCM between clients on the same channel.
Networking: Valve GameNetworkingSockets (GNS). Audio: RtAudio. CLI flags: cxxopts. Deps come from vcpkg manifests.

Read `docs/technical-design.md` for how the parts fit together, and `docs/roadmap/README.md` for the phase plan and decisions already made.

## Layout

- `VoiceChatServer/` — server executable. Single-threaded poll loop.
- `VoiceChatClient/` — CLI client executable. Main thread polls the network; the RtAudio callback thread captures and plays audio.
- `Common/Messages/` — wire messages, `MessageType` enum, `PROTOCOL_VERSION`.
- `Common/Serialization/` — bit stream and `Serialize(Stream&)` helpers. Header-only. Both executables list these headers in their `CMakeLists.txt`.
- `docs/roadmap/` — phase docs. One phase per session; update the status table and "Notes for later phases" when a phase ends.

Everything builds from the root `CMakeLists.txt` (IDE, CI, Docker). The folders don't build on their own.

## Dependencies

One manifest: root `vcpkg.json`.
- Used by both programs → top-level `dependencies`. Find it once in the root `CMakeLists.txt` (GNS targets are global, so a second `find_package` fails).
- Used by one program → that program's feature (`server` or `client`). Add `find_package` and `target_link_libraries` in that folder's `CMakeLists.txt`.
- `VOICECHAT_BUILD_SERVER` / `VOICECHAT_BUILD_CLIENT` (both ON by default) turn on each program and its vcpkg feature. Docker sets `VOICECHAT_BUILD_CLIENT=OFF`, so client libraries never reach the server image.

## Build (Windows)

`cmake --preset debug` then `cmake --build --preset debug` (output in `out/build/debug/`). Presets: `debug`, `release` (Ninja), `vs` (Windows only), which generates `out/build/vs/SimpleVoiceChat.sln` with both programs, and `xcode` (macOS only), which generates `out/build/xcode/SimpleVoiceChat.xcodeproj`. Rider 2024.1 can't load CMake directly, so it opens that `.sln`. Only the root calls `project()`; a `project()` in a folder makes the VS generator write an extra `.sln`.

Link GNS through `${GNS_LIB}` (set in the root). vcpkg builds GNS shared on Windows and static on Linux, so the target name differs.

From a plain shell, import the VS dev environment first or headers go missing. vcvars also overwrites `VCPKG_ROOT` with VS's bundled vcpkg (which fails on a manifest with no baseline), so set it back to `C:\Work\OpenSource\vcpkg` afterwards. Rider and VS handle the environment themselves.

Server in Docker: `docker build -t voice-chat-server .` then `docker run -p 27020:27020/udp voice-chat-server`.

## Testing

No automated tests. CI (`.github/workflows/`) builds both programs from the root on Windows and Ubuntu.
Smoke test by hand: start the server, connect two clients on localhost, watch both logs.
`-DVOICECHAT_AUDIO_STATS` turns on packet and sample stats in the client send path.

## Rules

- **Any wire change bumps `PROTOCOL_VERSION`** (`Common/Messages/MessageTypes.h`). This includes changing a field's encoding. Server and client check for strict equality.
- Each message has one `template<typename Stream> bool Serialize(Stream&)` that does write, read, and measure. Never memcpy structs onto the wire.
- Range-check every count and length on read (`serialize_*_range`). Treat payloads as untrusted.
- The first wire byte is the `uint8_t` type. Ranges: requests 1–63, events 64–127, data 128+. Values 2 and 3 stay unused.
- The server's audio relay does not decode. It reads `count` from wire bytes 5–8 and overwrites `senderId` at bytes 1–4 in place. If the audio layout moves, update the `AUDIO` case in `VoiceChatServer/Server.cpp` too.
- New headers in `Common/` must be added to both `CMakeLists.txt` files.
- Little-endian hosts only (compile-time `#error`).
