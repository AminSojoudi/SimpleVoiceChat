# Task completion

- Build both server and client (vcvars command in `mem:suggested_commands`) — a `Common/` change affects both.
- Only automated test: `out/build/debug/VoiceChatCore/SampleRingTest.exe`. No linter. For protocol/network changes, smoke test by hand: server + two clients on localhost, check logs. Client stdin takes `m`, `l`, `c <n>`, `q`, so a piped script can drive it headless; the server prints stats with `\r`, filter them out of its log.
- Wire change → `PROTOCOL_VERSION` bumped and phase-1 wire table in `docs/roadmap/phase-1-protocol-presence.md` updated.
- Phase end → update `docs/roadmap/README.md` status table and the phase doc's "Notes for later phases"; refresh `docs/technical-design.md` if architecture changed.
