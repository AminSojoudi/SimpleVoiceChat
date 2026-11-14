# Conventions

- Messages: one `template<typename Stream> bool Serialize(Stream&)` per struct serves Write/Read/Measure streams. Never memcpy structs to the wire. Range-check counts on read.
- Message type byte ranges: requests 1–63, events 64–127, data 128+; values 2 and 3 stay unused.
- Naming: PascalCase types and methods, camelCase fields/locals, UPPER_SNAKE enum values. "client list" not "roster" in code.
- Comments and names in plain, simple grammar (user preference). Less code wins; delete unused code (project unreleased).
- Server logs with `printf`. Core never prints: LOG events only (other threads queue under a mutex). The CLI prints only from its event callback.
- Exception to UPPER_SNAKE: `LogLevel` values are `Error/Warning/Info/Debug` (windows.h defines `ERROR`).
- Design decisions (schema, deps, module boundaries) go to the user before implementing.
