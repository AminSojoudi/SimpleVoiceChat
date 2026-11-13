# Conventions

- Messages: one `template<typename Stream> bool Serialize(Stream&)` per struct serves Write/Read/Measure streams. Never memcpy structs to the wire. Range-check counts on read.
- Message type byte ranges: requests 1–63, events 64–127, data 128+; values 2 and 3 stay unused.
- Naming: PascalCase types and methods, camelCase fields/locals, UPPER_SNAKE enum values. "client list" not "roster" in code.
- Comments and names in plain, simple grammar (user preference). Less code wins; delete unused code (project unreleased).
- Logging is `printf` today; phase 2 Core must not print (log callback instead).
- Design decisions (schema, deps, module boundaries) go to the user before implementing.
