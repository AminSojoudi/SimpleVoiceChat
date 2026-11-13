# Suggested commands (Windows)

- Build from repo root: `cmake --preset debug` then `cmake --build --preset debug` (output `out/build/debug/`; presets `debug`, `release`).
- Plain shell: import vcvars64 first (else missing headers), then reset `VCPKG_ROOT=C:\Work\OpenSource\vcpkg` — vcvars points it at VS's bundled vcpkg, which fails on a manifest with no baseline.
  `cmd /c "call \"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" && set VCPKG_ROOT=C:\Work\OpenSource\vcpkg && cmake --build --preset debug"`
- Server-only build: add `-DVOICECHAT_BUILD_CLIENT=OFF` at configure.
- Run: `VoiceChatServer --port 27020 --sample-rate 44100`; `VoiceChatClient --address 127.0.0.1 --port 27020 --channel 0 --name foo`. Both take `--help`.
- Docker server: `docker build -t voice-chat-server .` / `docker run -p 27020:27020/udp voice-chat-server`.
- Audio stats build: add `-DVOICECHAT_AUDIO_STATS`.
