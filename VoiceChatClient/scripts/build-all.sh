#!/usr/bin/env bash
# Build VoiceChatClientPlugin for hosts this script can target from the current OS.
# Version source: repository root VERSION (bump when Common message layouts change).
#
# Usage:
#   ./build-all.sh              # default: this OS + optional Android if NDK set
#   ./build-all.sh --no-unity-copy
#   ./build-all.sh --android-only
#   ./build-all.sh --skip-android
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLIENT_DIR="$(dirname "$SCRIPT_DIR")"
REPO_ROOT="$(dirname "$CLIENT_DIR")"
VERSION="$(tr -d ' \r\n' < "$REPO_ROOT/VERSION")"
echo "VoiceChat VERSION=$VERSION (edit $REPO_ROOT/VERSION when protocol / messages change)"

: "${VCPKG_ROOT:?Set VCPKG_ROOT}"
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
COPY="ON"
ANDROID_ONLY=0
SKIP_ANDROID=0
for arg in "$@"; do
  case "$arg" in
    --no-unity-copy) COPY="OFF" ;;
    --android-only) ANDROID_ONLY=1 ;;
    --skip-android) SKIP_ANDROID=1 ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

cmake_configure_build() {
  local out="$1"
  shift
  cmake -B "$out" -S "$CLIENT_DIR" "$@"
  cmake --build "$out" --config Release --target VoiceChatClientPlugin
}

build_android_all() {
  [[ -n "${ANDROID_NDK_HOME:-}" ]] || { echo "ANDROID_NDK_HOME not set; skipping Android."; return 0; }
  for abi in arm64-v8a armeabi-v7a x86_64; do
    echo ""
    echo "=== Android $abi ==="
    "$SCRIPT_DIR/build-plugin-android.sh" "$abi" "$COPY"
  done
}

host_macos_arch() {
  uname -m
}

if [[ "$ANDROID_ONLY" -eq 1 ]]; then
  build_android_all
  exit 0
fi

OS="$(uname -s)"
case "$OS" in
  Darwin)
    OSX_ARCH="$(host_macos_arch)"
    if [[ "$OSX_ARCH" != "arm64" && "$OSX_ARCH" != "x86_64" ]]; then
      OSX_ARCH=arm64
    fi
    echo ""
    echo "=== macOS ($OSX_ARCH dylib for Unity) ==="
    cmake_configure_build "$CLIENT_DIR/build-all/macos-$OSX_ARCH" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      "-DCMAKE_OSX_ARCHITECTURES=$OSX_ARCH" \
      "-DVOICECHAT_COPY_PLUGIN_TO_UNITY=$COPY"

    echo ""
    echo "=== iOS (device, static .a) ==="
    cmake_configure_build "$CLIENT_DIR/build-ios-arm64" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DVCPKG_TARGET_TRIPLET=arm64-ios \
      "-DVOICECHAT_COPY_PLUGIN_TO_UNITY=$COPY"
    ;;
  Linux)
    echo ""
    echo "=== Linux x86_64 ==="
    cmake_configure_build "$CLIENT_DIR/build-all/linux-x64" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      "-DVOICECHAT_COPY_PLUGIN_TO_UNITY=$COPY"
    ;;
  *)
    echo "Unsupported OS for build-all.sh: $OS (use build-all.ps1 on Windows)"
    exit 1
    ;;
esac

if [[ "$SKIP_ANDROID" -eq 0 ]]; then
  build_android_all
fi

echo ""
echo "On Windows, also run: VoiceChatClient/scripts/build-all.ps1"
