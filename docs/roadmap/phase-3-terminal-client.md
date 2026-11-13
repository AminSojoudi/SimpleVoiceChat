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
- **Options panel** (added 2026-11-12): opened with a key (e.g. `o`), lists the runtime options with their current values: mute, loopback, channel. Each change goes through Core's options API (phase 2 scope item 4). Direct shortcuts stay too: `m` mute, `l` loopback. New options added to Core later should show up here.
- **Status bar**: connection state, channel, server address, maybe sent/received byte counters (Core has counters ambient in the old code — check what survived extraction).
- **Event log area**: joins, leaves, errors — fed from Core's log/event callbacks. Nothing may print to stdout directly; FTXUI owns the screen.
- **Clean exit**: `q` / Ctrl-C disconnects cleanly via Core's shutdown path.

## Design notes

- FTXUI's reactive re-render model fits this: state lives in a view-model struct updated from Core callbacks; the render function draws from it. Animations (pulsing talk indicator) come from re-rendering on a timer tick.
- Threading: keep it simple — one UI loop that calls `session.Update()` (which fires Core callbacks on that same thread) and then refreshes the screen. FTXUI's `ftxui::Loop` covers this: own the loop, call `loop.RunOnce()` (non-blocking) then `session.Update()`, until `loop.HasQuitted()`. No extra thread needed.

## Libraries to reuse (researched 2026-11-12)

Don't build widgets from scratch. FTXUI already has almost everything the features above need.

**FTXUI** — keep it (decided in the roadmap README). MIT, C++17, most-used C++ TUI library, Windows supported. vcpkg port is `ftxui` (6.1.9 in the local vcpkg as of 2026-11-12; upstream is at 7.0.x — check the API docs match the version vcpkg installs). Docs: https://arthursonzogni.github.io/FTXUI/

What maps to which feature:

| Feature | FTXUI pieces |
|---|---|
| Layout (panels, status bar) | `Container::Vertical/Horizontal`, `ResizableSplit*`, `Window`, elements `vbox`/`hbox`/`flexbox`/`border`/`separator` |
| Client list panel | `Renderer` over the view-model, `table` or `vbox` rows, `color`/`bold` for the talking/muted state |
| Talking indicator + mic level | `gauge` (level meter), `spinner` or colour pulse; `RequestAnimationFrame()` and `animation::Animator` for animation |
| Options panel (mute, loopback, channel) | `Modal` for the popup, `Checkbox` for mute/loopback, `Input` (or `Dropdown`/`Slider`) for channel, `Button` to close |
| Keyboard shortcuts (`m`, `l`, `o`, `q`) | `CatchEvent` around the root component |
| Event log | `vbox` of `paragraph`s inside `yframe` with `focusPositionRelative` for scrolling |
| Connect screen | `Input` + `Button`; `Tab` or `Maybe` to switch between connect and chat views |

Third-party FTXUI components (listed in the FTXUI README; small repos, not in vcpkg — copy the file in if one is useful, don't add a dependency):
- `ftxui-ip-input` (https://github.com/mingsheng13/ip-input-ftxui) — IP address input, fits the connect screen.
- `ftxui-grid-container` (https://github.com/mingsheng13/grid-container-ftxui) — grid navigation, only if the layout needs it.

**Alternatives looked at, not chosen:**
- **tvision** (magiblot's Turbo Vision port; MIT, in vcpkg) — overlapping windows and menu bars in the classic DOS style. Heavier, older OOP API, less fit for a small reactive view.
- **PDCurses / ncurses** (vcpkg) — low level, different library per platform. We'd be writing the widgets FTXUI already has.
- **notcurses, ImTui, FINAL CUT** — no vcpkg port, and Windows support is weak or unclear.

Not needed: `termcolor`, `indicators`, `tabulate` (FTXUI covers colours, progress bars, and tables).
- Add FTXUI to the root `vcpkg.json` under a new `tui` feature (or `client`), with a matching `VOICECHAT_BUILD_TUI` switch in the root `CMakeLists.txt`. Wire the new target like the existing ones (see `VoiceChatClient/CMakeLists.txt` as the template).
- Keep `VoiceChatClient` (plain CLI) alive as the minimal reference client; the TUI is a separate target.

## Out of scope

- Text chat (fun later, protocol can grow it).
- Channel browsing (listing which channels exist or how many people are in each). Switching by typing a channel number is in scope through the options panel.
- Windows Terminal vs legacy console quirks beyond what FTXUI handles.

## Acceptance criteria

- Two TUI clients + server on localhost: each shows the other in the client list with correct name; talking indicator lights on the listener's screen while the other speaks; mute keypress stops audio and flips the muted badge on both screens.
- A third client joining/leaving another channel changes the online count but not the channel's client list.
- Options panel: toggling loopback lets a lone client hear itself; switching channel moves the client out of one client list and into the other on both screens.
- Server killed mid-session → TUI shows disconnected state gracefully (no garbage, no crash).
- No stray text ever corrupts the TUI layout (all logging goes through the log panel).
