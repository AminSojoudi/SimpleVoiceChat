# Tech stack

- C++17 (no bump; `if constexpr` used, no `std::endian` → big-endian guarded by `#error`).
- CMake ≥ 3.20, one root project + `CMakePresets.json`. vcpkg manifest mode with ONE root `vcpkg.json`: shared deps top-level, program-only deps under `server` / `client` features, picked by `VOICECHAT_BUILD_SERVER` / `VOICECHAT_BUILD_CLIENT`. GNS is found once in root (global targets).
- GameNetworkingSockets (UDP transport, all sends ReliableNoNagle), RtAudio (client only, duplex SINT16 stream), cxxopts (CLI flags; user chose it over CLI11/Boost).
- Planned: FTXUI for the phase-3 TUI.
- Dev: Windows + Visual Studio 2022 / Rider. CI: GitHub Actions builds root (both programs) on Windows and Ubuntu. Server Docker image on Ubuntu 22.04, client feature off.
