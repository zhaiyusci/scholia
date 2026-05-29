[CmdletBinding()]
param(
    [string] $WorkspaceRoot = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (!$WorkspaceRoot) {
    $WorkspaceRoot = Split-Path -Parent $repoRoot
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)

$texBin = Join-Path $WorkspaceRoot "TinyTeX\bin\windows"
$log = Join-Path $WorkspaceRoot "build-logs\tinytex-path.txt"
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$parts = @()
if ($userPath) {
    $parts = $userPath -split ";" | Where-Object { $_ }
}
if ($parts -notcontains $texBin) {
    $parts = @($texBin) + $parts
    [Environment]::SetEnvironmentVariable("Path", ($parts -join ";"), "User")
}
[Environment]::GetEnvironmentVariable("Path", "User") | Set-Content -LiteralPath $log -Encoding UTF8
Write-Host "TinyTeX PATH entry is configured for user $env:USERNAME"
Write-Host "PATH log: $log"
