# Task completion

- Build both server and client (vcvars command in `mem:suggested_commands`) — a `Common/` change affects both.
- No automated tests or linter. For protocol/network changes, smoke test by hand: server + two clients on localhost, check logs.
- Wire change → `PROTOCOL_VERSION` bumped and phase-1 wire table in `docs/roadmap/phase-1-protocol-presence.md` updated.
- Phase end → update `docs/roadmap/README.md` status table and the phase doc's "Notes for later phases"; refresh `docs/technical-design.md` if architecture changed.
