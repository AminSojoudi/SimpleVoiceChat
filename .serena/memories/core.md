# SimpleVoiceChat core

- Repo-root `CLAUDE.md` has layout, build, dependency, and protocol rules; `docs/technical-design.md` has the architecture; `docs/roadmap/` has the phase plan (one phase per session, update status table + "Notes for later phases" when done).
- Source map: `VoiceChatServer/` (single-threaded relay), `VoiceChatCore/` (client library, `VoiceChatSession.h` is the only public header; lock-free `SampleRing.h` + `SampleRingTest`), `VoiceChatClient/` (thin CLI over Core: prints events, stdin commands), `Common/Messages` (wire messages, `PROTOCOL_VERSION`), `Common/Serialization` (header-only bit streams).
- Single root CMake project; folders don't build alone. New `Common/` headers go in `VoiceChatServer/` and `VoiceChatCore/` `CMakeLists.txt` (CLI doesn't include Common except `AudioStats.h`). Core + CLI build only with `VOICECHAT_BUILD_CLIENT`. Deps rules: `mem:tech_stack`.
- Invariants:
  - Any wire change bumps `PROTOCOL_VERSION` (strict equality check). Currently 2.
  - Server audio relay patches bytes 1–4 (senderId) and reads bytes 9–12 (count) of the raw buffer (bytes 5–8 = client sequence); size must equal 13 + 2·count. Moving the audio layout means updating `Server.cpp` AUDIO case.
  - Server state is static. Client side is `VoiceChatSession`, one per process (file-static pointer for GNS callbacks, asserted). Events fire only inside `Update()` on the caller's thread; audio thread never logs itself but sends from the callback (GNS SendMessages locks/allocates internally). Senders gate on silence: audio goes out only while the mic is above `TalkingLevel` or within the 200 ms hold.
- Commands: `mem:suggested_commands`. Code style + serialization pattern: `mem:conventions`. Done-checklist: `mem:task_completion`.
