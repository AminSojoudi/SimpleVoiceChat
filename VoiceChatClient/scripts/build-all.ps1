<#
.SYNOPSIS
    Build the Unity native plugin for every platform this Windows machine can produce.

.DESCRIPTION
    Reads protocol version from repository root VERSION (bump when Common message layouts change).

    - Windows x64 DLL + vcpkg runtime DLLs -> UnityVoiceChatClient/Assets/Plugins/VoiceChat/Windows/x86_64
    - Android ABIs -> .../Plugins/VoiceChat/Android/libs/<abi>/ (needs ANDROID_NDK_HOME, Ninja)

    For Linux .so, macOS .dylib, and iOS .a run scripts/build-all.sh on Linux or macOS (iOS only on macOS).

.PARAMETER NoUnityCopy
    Set CMake VOICECHAT_COPY_PLUGIN_TO_UNITY=OFF (CI / artifact-only output under build-all/).

.PARAMETER SkipWindows
.PARAMETER SkipAndroid
#>
param(
    [switch]$NoUnityCopy,
    [switch]$SkipWindows,
    [switch]$SkipAndroid
)

$ErrorActionPreference = "Stop"
$ClientDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$RepoRoot = Resolve-Path (Join-Path $ClientDir "..")
$VersionPath = Join-Path $RepoRoot "VERSION"
$Version = (Get-Content $VersionPath -Raw).Trim()
Write-Host "VoiceChat VERSION=$Version  (edit $VersionPath when protocol / messages change)" -ForegroundColor Green

$VcpkgRoot = $env:VCPKG_ROOT
if (-not $VcpkgRoot) {
    Write-Error "Set VCPKG_ROOT to your vcpkg root."
}
$Toolchain = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
$CopyFlag = if ($NoUnityCopy) { "OFF" } else { "ON" }

if (-not $SkipWindows) {
    if (-not ($IsWindows -or $PSVersionTable.Platform -eq "Win32NT")) {
        Write-Warning "Skipping Windows build (not on Windows)."
    } else {
        $WinOut = Join-Path $ClientDir "build-all/windows-x64"
        Write-Host "`n=== Windows x64 ===" -ForegroundColor Cyan
        cmake -B $WinOut -S $ClientDir -G "Visual Studio 17 2022" -A x64 `
            "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
            "-DVOICECHAT_COPY_PLUGIN_TO_UNITY=$CopyFlag"
        cmake --build $WinOut --config Release --target VoiceChatClientPlugin
    }
}

if (-not $SkipAndroid) {
    if (-not $env:ANDROID_NDK_HOME) {
        Write-Warning "ANDROID_NDK_HOME not set — skipping Android ABIs."
    } else {
        foreach ($abi in @("arm64-v8a", "armeabi-v7a", "x86_64")) {
            Write-Host "`n=== Android $abi ===" -ForegroundColor Cyan
            if ($NoUnityCopy) {
                & (Join-Path $PSScriptRoot "build-plugin-android.ps1") -Abi $abi -NoUnityCopy
            } else {
                & (Join-Path $PSScriptRoot "build-plugin-android.ps1") -Abi $abi
            }
        }
    }
}

Write-Host "`nDone. On macOS/Linux also run: VoiceChatClient/scripts/build-all.sh" -ForegroundColor Yellow
