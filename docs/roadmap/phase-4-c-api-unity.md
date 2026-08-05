# Phase 4: C API + Unity plugin (future)

## Goal

Expose `VoiceChatCore` through a flat C ABI so non-C++ hosts — first target: Unity via P/Invoke — can use the full client feature set (connect, client list, talking events, mute, audio).

This phase is intentionally under-specified: do not design it in detail until Unity work actually starts. It exists so earlier phases keep it cheap.

## What earlier phases already guarantee

- Core is instance-based with a flat callback interface — the same shape as a C API (opaque handle + functions + function-pointer callbacks + user-data pointer).
- Core never prints or owns a UI; logging is a callback.
- Callbacks fire only from `Update()` on the caller's thread — friendly to Unity's main-thread-only scripting rules.

## Rough shape (sketch, not a spec)

- `VoiceChatCoreC/` (or a subfolder of Core) building a **shared library** (DLL) with `extern "C"` functions: create/destroy session, connect/disconnect, update, set muted, client list queries into caller-provided buffers, register callbacks with a `void* userData`.
- Strings cross the boundary as UTF-8 `const char*`; structs crossing the boundary get fixed layouts (`#pragma pack` or static_asserts).
- Audio device handling inside Core (RtAudio) initially; later Unity may want to feed/pull PCM itself (Unity `AudioSource`/`OnAudioFilterRead`) — that would need a "headless audio" mode in Core where the host pushes mic samples and pulls playback samples. Note this as the one Core design point worth keeping in mind early.
- C# side: a thin P/Invoke wrapper class + a MonoBehaviour that calls `Update()`.

## Acceptance (when it happens)

- A minimal Unity scene connects to the server, shows the client list in a UI text element, and voice works both directions alongside a native TUI client on the same channel.
