
[![Windows CI](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-windows-platform.yml/badge.svg?branch=main)](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-windows-platform.yml)
[![Ubuntu CI](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-ubuntu-platform.yml/badge.svg?branch=main)](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-ubuntu-platform.yml)

# SimpeVoiceChat

Simple C++ UDP voice chat using Valve [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets) and [RtAudio](https://github.com/thestk/rtaudio) for the desktop CLI client. The **Unity** player loads a small native plugin (`VoiceChatClientPlugin`) that uses the same protocol as the server.

---

## Quick start (server)

Build and run the server with Docker:

```bash
docker build -t voice-chat-server .
docker run -p 27020:27020/udp voice-chat-server
```

---

## Protocol and version

- **Single source of truth:** repository root [`VERSION`](VERSION) (semantic version, e.g. `1.0.0`).
- Bump **`VERSION`** when you change **`Common/`** message layouts or anything that must stay in sync between **server**, **native plugin**, and **Unity**.
- The native plugin exposes **`VC_GetVersionString()`**; C# can read **`VoiceChatUnityClient.NativePluginVersion`**.
- The server prints **`VoiceChat protocol version: …`** on startup (from the same `VERSION` at compile time).
- After a local plugin build with Unity copy enabled, **`UnityVoiceChatClient/Assets/Plugins/VoiceChat/VoiceChatPluginVersion.txt`** is updated (gitignored).

---

## Unity native plugin layout (`Assets/Plugins/VoiceChat`)

All VoiceChat binaries and their Windows vcpkg **runtime DLLs** are placed under **`VoiceChat/`** so they do not mix with other native plugins and so **only this subtree is gitignored** for `.dll` / `.so` / etc. Put **other** third-party plugins directly under **`Assets/Plugins/`** (or your usual folders); those files are **not** ignored by this repo’s rules.

| Platform | Unity folder |
|----------|----------------|
| Windows (x64 Editor / standalone) | `Plugins/VoiceChat/Windows/x86_64/` — `VoiceChatClientPlugin.dll` + vcpkg DLLs (OpenSSL, protobuf, Abseil, GameNetworkingSockets, …) |
| Linux | `Plugins/VoiceChat/Linux/x86_64/` — `libVoiceChatClientPlugin.so` |
| macOS | `Plugins/VoiceChat/macOS/` — `libVoiceChatClientPlugin.dylib` |
| Android | `Plugins/VoiceChat/Android/libs/<abi>/` — `libVoiceChatClientPlugin.so` |
| iOS | `Plugins/VoiceChat/iOS/` — static `.a` (you must also link GNS/OpenSSL/protobuf/Abseil; see scripts) |

Android permissions for this plugin are declared in **`Plugins/VoiceChat/Android/AndroidManifest.xml`** (merged by Unity).

Override the install root in CMake with **`UNITY_VOICECHAT_PLUGINS`** if needed.

---

## Building the Unity native plugin

### Prerequisites (Windows — Windows + Android)

- **Visual Studio 2022** with **Desktop development with C++**
- **CMake**
- **Ninja** (required for Android configures). Either `winget install Ninja-build.Ninja`, or enable **C++ CMake tools for Windows** in Visual Studio Installer (Ninja ships under `...\CommonExtensions\Microsoft\CMake\Ninja`). The Android script prepends that folder to `PATH` when `ninja` is not already found.
- **vcpkg:** clone, run `bootstrap-vcpkg.bat`, set **`VCPKG_ROOT`**
- **Android NDK** (for `.so`): set **`ANDROID_NDK_HOME`** to the NDK root (must contain `build/cmake/android.toolchain.cmake`). **Do not use Unity’s NDK under `C:\Program Files\...`** for vcpkg builds—paths with spaces break OpenSSL. Install the NDK via **Android Studio** (default: `%LOCALAPPDATA%\Android\Sdk\ndk\<version>`) or put it under e.g. `C:\Android\ndk\<version>`. The script **`build-plugin-android.ps1`** will warn and switch to a space-free NDK under `%LOCALAPPDATA%\Android\Sdk\ndk` if it finds one.
- **Android triplets:** vcpkg’s default `*-android` triplets use **static** libraries; **GameNetworkingSockets** must be built **shared**. This repo adds **`cmake/vcpkg-triplets/`** overlays (wired from **`VoiceChatClient/vcpkg.json`** → `vcpkg-configuration.overlay-triplets`). They also set **`VCPKG_BUILD_TYPE=release`** so OpenSSL is not built in debug on Android (the debug `make install` step often breaks on Windows + MSYS `make`). **`cmake/vcpkg-ports/gamenetworkingsockets`** is an overlay port (same `vcpkg.json` → `overlay-ports`) that patches upstream CMake so **`CMAKE_SYSTEM_NAME=Android`** is recognized. After changing triplets or ports, delete **`VoiceChatClient/build-android-*`** and reconfigure; if OpenSSL was half-installed, remove **`vcpkg/buildtrees/openssl`** or run **`vcpkg remove openssl:arm64-android`** then rebuild.

### One command (Windows: DLL + all Android ABIs)

From PowerShell:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:ANDROID_NDK_HOME = "C:\path\to\Android\Sdk\ndk\<version>"
cd SimpleVoiceChat\VoiceChatClient\scripts
.\build-all.ps1
```

This configures **`VOICECHAT_COPY_PLUGIN_TO_UNITY=ON`** by default and copies outputs into **`UnityVoiceChatClient/Assets/Plugins/VoiceChat/...`**.

- **`-NoUnityCopy`** — build only under `VoiceChatClient/build-all/` / `build-android-*` (no Unity tree updates).
- **`-SkipWindows`** / **`-SkipAndroid`** — build only one side.

### Android only (one ABI)

```powershell
.\build-plugin-android.ps1 -Abi arm64-v8a
```

### macOS / Linux / iOS

On a Mac or Linux host:

```bash
export VCPKG_ROOT=/path/to/vcpkg
export ANDROID_NDK_HOME=/path/to/ndk   # optional, for Android from Unix
cd VoiceChatClient/scripts
chmod +x build-all.sh
./build-all.sh
```

`./build-all.sh --no-unity-copy` disables copying into Unity. **`--android-only`** builds only Android ABIs.

### vcpkg runtime DLLs (Windows)

After the plugin DLL is copied into **`Plugins/VoiceChat/Windows/x86_64/`**, CMake runs vcpkg’s **`applocal.ps1`** so dependent DLLs from **`vcpkg_installed/<triplet>/bin`** (or **`debug/bin`** for Debug) are copied **next to** `VoiceChatClientPlugin.dll`. **`VCPKG_ROOT`** must be set at build time.

Requires **PowerShell** or **pwsh** on `PATH`. If deploy fails, copy DLLs manually from your build tree’s `vcpkg_installed\x64-windows\bin` into the same Unity folder as the plugin.

Android/Linux/macOS generally do not use this step (dependencies are resolved differently); if an Android build misses `.so` dependencies, add them under the same `libs/<abi>` folder or link statically.

### Packaging

```powershell
cd VoiceChatClient\scripts
.\package-unity-plugins.ps1
```

Creates **`VoiceChatClient/dist/VoiceChatUnityPlugins-v<VERSION>.zip`** from **`Assets/Plugins/VoiceChat/`**.

### CI

Workflow **[`.github/workflows/voicechat-native-plugins.yml`](.github/workflows/voicechat-native-plugins.yml)** builds the plugin on Windows, Linux, macOS, and Android matrix runners and uploads versioned artifacts (no Unity tree in CI).

---

## Desktop CLI client

Configure **`VoiceChatClient`** with the vcpkg toolchain (manifest mode uses [`VoiceChatClient/vcpkg.json`](VoiceChatClient/vcpkg.json)), build **`VoiceChatClient`** and run against the server address/port/channel.

---

# TODO

- [X] Make Client Multiplatform
- [X] Separate Network thread from Audio thread
- [ ] Improve Client/Server connection handling with proper logs
- [ ] Check Projects for memory leaks and consumption
- [X] improve architecture
- [X] Dockerize Server and push to DockerHub
- [ ] Make Releases for Client in Github
- [X] Automate Builds
- [ ] Add log Level functionality to server and client
- [ ] improve resiliency to different network bandwidths and network changes
- [X] Calculate bandwith usage
- [ ] Optimise bandwith
- [ ] Add some terminal voice visualization to client
