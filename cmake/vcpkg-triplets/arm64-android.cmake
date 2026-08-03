# Overlay: GameNetworkingSockets requires dynamic libraries (vcpkg_check_linkage ONLY_DYNAMIC_LIBRARY).
# Upstream arm64-android uses VCPKG_LIBRARY_LINKAGE static, which fails for this port.
# Release-only: OpenSSL's *-android-dbg "make install" often fails on Windows hosts; skip debug artifacts.
set(VCPKG_BUILD_TYPE release)
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 28)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-linux-android")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=arm64-v8a)
