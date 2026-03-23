# Build libVoiceChatClientPlugin.so for Unity (Android). Requires: Ninja, CMake, VCPKG_ROOT, ANDROID_NDK_HOME.
param(
    [ValidateSet("arm64-v8a", "armeabi-v7a", "x86_64")]
    [string]$Abi = "arm64-v8a",
    [switch]$NoUnityCopy
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ClientDir = Split-Path -Parent $ScriptDir

$VcpkgRoot = $env:VCPKG_ROOT
if (-not $VcpkgRoot) {
    Write-Error "Set VCPKG_ROOT to your vcpkg installation root."
}
$Ndk = $env:ANDROID_NDK_HOME
if (-not $Ndk) {
    Write-Error "Set ANDROID_NDK_HOME to your Android NDK path (e.g. .../Android/Sdk/ndk/26.1.10909125)."
}

$triplets = @{
    "arm64-v8a"   = "arm64-android"
    "armeabi-v7a" = "arm-neon-android"
    "x86_64"      = "x64-android"
}
$triplet = $triplets[$Abi]

$env:ANDROID_NDK_HOME = $Ndk

$BuildDir = Join-Path $ClientDir "build-android-$Abi"
$VcpkgToolchain = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
$CopyFlag = if ($NoUnityCopy) { "OFF" } else { "ON" }

& cmake -B $BuildDir -S $ClientDir -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain" `
    "-DVCPKG_TARGET_TRIPLET=$triplet" `
    "-DANDROID_ABI=$Abi" `
    "-DANDROID_PLATFORM=android-24" `
    "-DVOICECHAT_COPY_PLUGIN_TO_UNITY=$CopyFlag"

& cmake --build $BuildDir --config Release --target VoiceChatClientPlugin

Write-Host "Built VoiceChatClientPlugin for $Abi (VOICECHAT_COPY_PLUGIN_TO_UNITY=$CopyFlag)."
