#!/usr/bin/env bash
# Build libVoiceChatClientPlugin.so for Unity (Android). Requires: Ninja, CMake, VCPKG_ROOT, ANDROID_NDK_HOME.
# Usage: build-plugin-android.sh [arm64-v8a|armeabi-v7a|x86_64] [ON|OFF]
#   Second arg is VOICECHAT_COPY_PLUGIN_TO_UNITY (default ON).
set -euo pipefail

ABI="${1:-arm64-v8a}"
COPY_FLAG="${2:-ON}"
case "$ABI" in
  arm64-v8a)    TRIPLET=arm64-android ;;
  armeabi-v7a)  TRIPLET=arm-neon-android ;;
  x86_64)       TRIPLET=x64-android ;;
  *) echo "Usage: $0 [arm64-v8a|armeabi-v7a|x86_64] [ON|OFF]"; exit 1 ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLIENT_DIR="$(dirname "$SCRIPT_DIR")"

: "${VCPKG_ROOT:?Set VCPKG_ROOT}"
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME}"

BUILD_DIR="$CLIENT_DIR/build-android-$ABI"
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
ANDROID_CHAINLOAD="$VCPKG_ROOT/scripts/toolchains/android.cmake"

cmake -B "$BUILD_DIR" -S "$CLIENT_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$ANDROID_CHAINLOAD" \
  -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
  -DVCPKG_CRT_LINKAGE=dynamic \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM=android-24 \
  "-DVOICECHAT_COPY_PLUGIN_TO_UNITY=$COPY_FLAG"

cmake --build "$BUILD_DIR" --config Release --target VoiceChatClientPlugin

echo "Built VoiceChatClientPlugin for $ABI (VOICECHAT_COPY_PLUGIN_TO_UNITY=$COPY_FLAG)."
