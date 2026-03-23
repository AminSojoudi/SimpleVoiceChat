# Zip native plugins under UnityVoiceChatClient/Assets/Plugins for release (uses repo root VERSION in archive name).
param(
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$ClientDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$RepoRoot = Resolve-Path (Join-Path $ClientDir "..")
$Plugins = Join-Path $RepoRoot "UnityVoiceChatClient/Assets/Plugins/VoiceChat"
$Version = (Get-Content (Join-Path $RepoRoot "VERSION") -Raw).Trim()
if (-not $OutputDir) {
    $OutputDir = Join-Path $ClientDir "dist"
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$zip = Join-Path $OutputDir "VoiceChatUnityPlugins-v$Version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $Plugins "*") -DestinationPath $zip -Force
Write-Host "Created $zip"
