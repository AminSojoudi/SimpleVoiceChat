# Build libVoiceChatClientPlugin.so for Unity (Android). Requires: Ninja, CMake, VCPKG_ROOT, ANDROID_NDK_HOME.
param(
    [ValidateSet("arm64-v8a", "armeabi-v7a", "x86_64")]
    [string]$Abi = "arm64-v8a",
    [switch]$NoUnityCopy
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ClientDir = Split-Path -Parent $ScriptDir

function Ensure-NinjaOnPath {
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        return
    }
    $vsNinjaDirs = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
    )
    foreach ($dir in $vsNinjaDirs) {
        $exe = Join-Path $dir "ninja.exe"
        if (Test-Path -LiteralPath $exe) {
            $env:PATH = "$dir;$env:PATH"
            Write-Host "Using Ninja from Visual Studio: $exe" -ForegroundColor DarkGray
            return
        }
    }
    Write-Error @"
CMake could not find Ninja on PATH.

Fix one of:
  1) Install Ninja and add it to PATH, e.g.  winget install Ninja-build.Ninja
  2) In Visual Studio Installer, enable component:  Desktop development with C++  ->  C++ CMake tools for Windows
     (bundles Ninja under CommonExtensions\Microsoft\CMake\Ninja)
"@
}

function Resolve-AndroidNdkHome {
    $ndk = $env:ANDROID_NDK_HOME
    if ([string]::IsNullOrWhiteSpace($ndk)) {
        return $null
    }
    $ndk = $ndk.TrimEnd('\', '/')
    # vcpkg OpenSSL (and other autotools ports) break when the NDK lives under "Program Files" (spaces in --sysroot, compiler paths).
    $hasSpaces = $ndk -match '\s'
    $underProgramFiles = $ndk -match '(?i)Program Files'
    if (-not $hasSpaces -and -not $underProgramFiles) {
        return $ndk
    }

    $candidates = @()
    $sdkNdks = Join-Path $env:LOCALAPPDATA "Android\Sdk\ndk"
    if (Test-Path -LiteralPath $sdkNdks) {
        $candidates += Get-ChildItem -LiteralPath $sdkNdks -Directory -ErrorAction SilentlyContinue | Sort-Object { $_.Name } -Descending
    }
    $alt = "C:\Android\ndk"
    if (Test-Path -LiteralPath $alt) {
        $candidates += Get-ChildItem -LiteralPath $alt -Directory -ErrorAction SilentlyContinue | Sort-Object { $_.Name } -Descending
    }

    foreach ($d in $candidates) {
        $p = $d.FullName
        if ($p -notmatch '\s' -and (Test-Path -LiteralPath (Join-Path $p "build\cmake\android.toolchain.cmake"))) {
            Write-Warning @"
ANDROID_NDK_HOME was under a path with spaces:
  $ndk
vcpkg (OpenSSL) often fails there. Using this NDK instead:
  $p
"@
            return $p
        }
    }

    Write-Error @"
ANDROID_NDK_HOME points to a path with spaces:
  $ndk

vcpkg's OpenSSL build for Android breaks when clang/sysroot paths contain spaces (common with Unity's NDK under Program Files).

Fix:
  1) Install an NDK with Android Studio: SDK Manager -> NDK (Side by side). Default path has no spaces, e.g.
     $sdkNdks\<version>
  2) Set ANDROID_NDK_HOME to that folder (must contain build\cmake\android.toolchain.cmake), e.g.
     `$env:ANDROID_NDK_HOME = `"$sdkNdks\26.1.10909125`"
  3) Or install the NDK under e.g. C:\Android\ndk\<version> with no spaces in the path.
"@
}

function Invoke-CMake {
    # Do not name this parameter "Args" — PowerShell's automatic $args breaks "@Args" splatting to cmake.
    param([string[]]$CMakeArguments)
    & cmake @CMakeArguments
    if (-not $?) {
        $code = $LASTEXITCODE
        if ($null -eq $code) { $code = 1 }
        exit $code
    }
}

Ensure-NinjaOnPath

$VcpkgRoot = $env:VCPKG_ROOT
if (-not $VcpkgRoot) {
    Write-Error "Set VCPKG_ROOT to your vcpkg installation root."
}

$Ndk = Resolve-AndroidNdkHome
if (-not $Ndk) {
    Write-Error "Set ANDROID_NDK_HOME to your Android NDK root (directory that contains build\cmake\android.toolchain.cmake)."
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
# Consumer CMake must chainload the NDK like vcpkg port builds do; otherwise project() cannot find CXX for Android.
$VcpkgAndroidChainload = Join-Path $VcpkgRoot "scripts/toolchains/android.cmake"
$CopyFlag = if ($NoUnityCopy) { "OFF" } else { "ON" }

Invoke-CMake -CMakeArguments @(
    "-B", $BuildDir,
    "-S", $ClientDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain",
    "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$VcpkgAndroidChainload",
    "-DVCPKG_TARGET_TRIPLET=$triplet",
    "-DVCPKG_CRT_LINKAGE=dynamic",
    "-DANDROID_ABI=$Abi",
    "-DANDROID_PLATFORM=android-24",
    "-DVOICECHAT_COPY_PLUGIN_TO_UNITY=$CopyFlag"
)

Invoke-CMake -CMakeArguments @(
    "--build", $BuildDir,
    "--config", "Release",
    "--target", "VoiceChatClientPlugin"
)

Write-Host "Built VoiceChatClientPlugin for $Abi (VOICECHAT_COPY_PLUGIN_TO_UNITY=$CopyFlag)."
