# Phase 1.5: Root build, Docker, and CI

Status: in progress (started 2026-11-12). Code is done and Docker is verified; GitHub CI is still open. Finish this before phase 2.

## Goal

One CMake project at the root that builds both programs, so an IDE opens them together. One vcpkg manifest with no duplicate dependency lists, while server dependencies stay separate from client ones. Docker and CI build from the same root.

## What was done (2026-11-12)

- **Root `CMakeLists.txt`** adds `VoiceChatServer/` and `VoiceChatClient/`. Only the root calls `project()` and sets C++17. A `project()` call in a folder makes the Visual Studio generator write an extra solution file, so the folders have none.
- **One root `vcpkg.json`.** Shared deps (GameNetworkingSockets, cxxopts) sit at the top level. Program-only deps go under the `server` feature (empty today) or the `client` feature (rtaudio). The folder manifests were deleted; the folders no longer build on their own.
- **Switches** `VOICECHAT_BUILD_SERVER` / `VOICECHAT_BUILD_CLIENT` (both ON) turn on each program and its vcpkg feature. They set `VCPKG_MANIFEST_FEATURES` before `project()`, because vcpkg reads it there.
- **GNS is found once, in the root.** Its CMake config makes global targets, so a second `find_package` fails. vcpkg builds GNS shared on Windows (`GameNetworkingSockets::shared`) and static on Linux (`::static`). The root stores whichever exists in `GNS_LIB`, and both programs link `${GNS_LIB}`.
- **`CMakePresets.json`**: `debug` and `release` (Ninja), `vs` (Windows only, generates `out/build/vs/SimpleVoiceChat.slnx`), `xcode` (macOS only, generates `out/build/xcode/SimpleVoiceChat.xcodeproj`). Rider 2024.1 can't load CMake directly, so it opens the `vs` solution. The old hand-made `VoiceChatServer.xcodeproj` was deleted.
- **Dockerfile** builds from the root with `-DVOICECHAT_BUILD_CLIENT=OFF`, so rtaudio never gets installed. It no longer runs a separate `vcpkg install`. Linux links statically, so the runtime image copies only the server binary (no `.so`, no `libssl3`).
- **`.dockerignore`** keeps local build output (`out/`, `build/`, IDE folders) out of the image context. The root `out/` holds a full vcpkg install, many GB.
- **CI** (`.github/workflows/cmake-{windows,ubuntu}-platform.yml`): both build the root project, so the server is built too. The empty `ctest` step was removed. File names are unchanged because the README badges point at them.
- **Linux compile fix.** `channel` in `ClientConfigMessage.h` and `ClientListMessages.h` was GNS's `int64` (`long long`), but `serialize_int64` takes `int64_t` (`long` on Linux). Changed both fields to `int64_t`. Wire bytes are the same, so no protocol bump. Nothing built the server on Linux before, so this went unnoticed.

## Verified

- Windows: `debug` (Ninja) and `vs` presets configure and build both programs; both run `--help`.
- Windows server-only configure (`-DVOICECHAT_BUILD_CLIENT=OFF`) builds the server and does not install rtaudio.
- Rider opens `out/build/vs/SimpleVoiceChat.slnx` and runs both programs.
- Docker: `docker build -t voice-chat-server .` succeeds and `docker run --rm voice-chat-server ./VoiceChatServer --help` runs. Image is 90 MB. Static linking works; the runtime stage needs no extra libraries.

## Dependency versions pinned (2026-11-12)

- `vcpkg.json` has `builtin-baseline` = vcpkg commit `af2c387` (2026-11-13). It pins GameNetworkingSockets **1.6.1** (was 1.5.0 locally), rtaudio **6.0.1**, cxxopts **3.3.1**. Local, CI, and Docker now build the same versions. The user decided to pin.
- GNS 1.5.0 → 1.6.1 needed no code changes: we only use core APIs (connect, listen, poll groups, send/receive, `RunCallbacks`). 1.5.1 fixed reliable-message bugs that matter here, since we send everything reliable.
- The local vcpkg (`C:\Work\OpenSource\vcpkg`) was moved to that commit and re-bootstrapped (tool 2026-10-29). A builtin baseline reads the version database from the checkout's working tree, so the checkout must be at or after the baseline commit.
- To update later: move the vcpkg checkout forward, run `vcpkg x-update-baseline` in the repo root, rebuild clean.

## Visual Studio 2026 (2026-11-12)

VS 2022 was removed from the dev machine and VS Community 2026 (version 18, MSVC 14.52) installed. The `vs` preset now uses the `Visual Studio 18 2026` generator. It writes `out/build/vs/SimpleVoiceChat.slnx` (the new XML solution format) instead of `.sln`. Both programs build. vcvars now lives at `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`.

## Still to do

1. **CI passes on GitHub.** Push the branch and check both workflows. Ubuntu builds the client too, which needs rtaudio's Linux audio deps through vcpkg. Watch for missing system packages (e.g. autoconf, libtool) on the runner.
2. **Rider with VS 2026.** Check that Rider opens `SimpleVoiceChat.slnx` and builds with the VS 2026 toolset. Rider 2024.1 predates both `.slnx` and VS 2026, so it likely needs an update. Newer Rider versions can also open the root `CMakeLists.txt` directly, which would make the `vs` preset optional.
3. **Xcode preset** is untested. Check `cmake --preset xcode` on a Mac when one is available.
4. **Leftover local files** (not tracked by git): `VoiceChatServer/CMakeSettings.json` and `VoiceChatClient/CMakeSettings.json` are VS settings for the old per-folder builds. Delete them.

All of this work was committed on `feature/terminal-client` on 2026-11-12 (including `.serena/` with its project config and memories).

## Notes for later phases

- New dependency: add it to the root `vcpkg.json` under the feature of the program that uses it (top level if both use it). Then `find_package` + `target_link_libraries` in that program's folder.
- Phase 3 adds the TUI as a new folder with its own feature (`tui`) and switch (`VOICECHAT_BUILD_TUI`). FTXUI goes under that feature.
- Building from a plain shell on Windows: import vcvars64, then set `VCPKG_ROOT` back to `C:\Work\OpenSource\vcpkg`. vcvars points it at VS's bundled vcpkg, a different checkout from the one the IDE uses.
