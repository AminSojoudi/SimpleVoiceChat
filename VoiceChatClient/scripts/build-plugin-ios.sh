#!/usr/bin/env bash
# Build libVoiceChatClientPlugin.a for Unity (iOS, device arm64). Run on macOS only.
# Install deps first, e.g.: vcpkg install gamenetworkingsockets:arm64-ios
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLIENT_DIR="$(dirname "$SCRIPT_DIR")"

: "${VCPKG_ROOT:?Set VCPKG_ROOT}"

BUILD_DIR="$CLIENT_DIR/build-ios-arm64"
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake -B "$BUILD_DIR" -S "$CLIENT_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_TARGET_TRIPLET=arm64-ios

cmake --build "$BUILD_DIR" --config Release --target VoiceChatClientPlugin

echo "Static library built. CMake copies to UnityVoiceChatClient/Assets/Plugins/iOS/ when VOICECHAT_COPY_PLUGIN_TO_UNITY is ON."
echo "Also copy GameNetworkingSockets, protobuf, OpenSSL, and Abseil static libraries from vcpkg installed/arm64-ios/lib into the same Unity Plugins/iOS folder so Xcode can link them."
