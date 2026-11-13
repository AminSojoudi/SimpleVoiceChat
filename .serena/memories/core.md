# SimpleVoiceChat core

- Repo-root `CLAUDE.md` has layout, build, dependency, and protocol rules; `docs/technical-design.md` has the architecture; `docs/roadmap/` has the phase plan (one phase per session, update status table + "Notes for later phases" when done).
- Source map: `VoiceChatServer/` (single-threaded relay), `VoiceChatClient/` (CLI client, 3 threads), `Common/Messages` (wire messages, `PROTOCOL_VERSION`), `Common/Serialization` (header-only bit streams).
- Single root CMake project; folders don't build alone. New `Common/` headers go in both folder `CMakeLists.txt`. Deps rules: `mem:tech_stack`.
- Invariants:
  - Any wire change bumps `PROTOCOL_VERSION` (strict equality check).
  - Server audio relay patches bytes 1–4 (senderId) and reads bytes 5–8 (count) of the raw buffer; moving the audio layout means updating `Server.cpp` AUDIO case.
  - Server and client state is static (process-global); phase 2 replaces client side with instance-based `VoiceChatCore`.
- Commands: `mem:suggested_commands`. Code style + serialization pattern: `mem:conventions`. Done-checklist: `mem:task_completion`.
