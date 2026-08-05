# Phase 3: Terminal UI client

## Goal

A new `VoiceChatTUI/` executable: a full-screen terminal voice chat client built on `VoiceChatCore` (phase 2) using **FTXUI** (via vcpkg). This is the flagship client.

## Prerequisites

- Phase 1 (client list/talking data on the wire) and phase 2 (Core library with callback API) are done.
- Check the "Notes for later phases" section of both docs for the final protocol and Core API surfaces.

## Features

- **Connect screen / CLI args**: reuse the existing cxxopts pattern (`VoiceChatClient/Config.cpp`) — address, port, channel, name, plus whatever phase 1/2 added.
- **Client list panel**: everyone in my channel (name, muted state), plus total online count on the server.
- **Talking indicators**: per-person indicator that lights while they talk (Core provides talking started/stopped events with ~200ms hold). Show who is talking right now.
- **My mic status**: an animated element while I'm talking (level meter or pulsing indicator — Core exposes local mic energy), plus a clear muted/unmuted state.
- **Mute toggle**: single keypress (e.g. `m`). Muted state visible to others (phase 1 carries it in config/client list).
- **Status bar**: connection state, channel, server address, maybe sent/received byte counters (Core has counters ambient in the old code — check what survived extraction).
- **Event log area**: joins, leaves, errors — fed from Core's log/event callbacks. Nothing may print to stdout directly; FTXUI owns the screen.
- **Clean exit**: `q` / Ctrl-C disconnects cleanly via Core's shutdown path.

## Design notes

- FTXUI's reactive re-render model fits this: state lives in a view-model struct updated from Core callbacks; the render function draws from it. Animations (pulsing talk indicator) come from re-rendering on a timer tick.
- Threading: keep it simple — one UI loop that calls `session.Update()` (which fires Core callbacks on that same thread) and then refreshes the screen. FTXUI's event loop and Core's poll need to interleave; planning should look at FTXUI's `ScreenInteractive` custom-loop / post-event options.
- Add FTXUI via vcpkg manifest/port; wire a new CMake target like the existing ones (see `VoiceChatClient/CMakeLists.txt` as the template).
- Keep `VoiceChatClient` (plain CLI) alive as the minimal reference client; the TUI is a separate target.

## Out of scope

- Text chat (fun later, protocol can grow it).
- Channel browsing/switching UI beyond a `--channel` arg (switching exists protocol-wise; a picker UI can come later).
- Windows Terminal vs legacy console quirks beyond what FTXUI handles.

## Acceptance criteria

- Two TUI clients + server on localhost: each shows the other in the client list with correct name; talking indicator lights on the listener's screen while the other speaks; mute keypress stops audio and flips the muted badge on both screens.
- A third client joining/leaving another channel changes the online count but not the channel's client list.
- Server killed mid-session → TUI shows disconnected state gracefully (no garbage, no crash).
- No stray text ever corrupts the TUI layout (all logging goes through the log panel).
