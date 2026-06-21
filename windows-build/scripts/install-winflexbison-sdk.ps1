[CmdletBinding()]
param(
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $Version = "2.5.25"
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (!$WorkspaceRoot) {
    $WorkspaceRoot = Join-Path (Split-Path -Parent $repoRoot) "windows_build"
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)

if (!$SdkPrefix) {
    $SdkPrefix = Join-Path $WorkspaceRoot "sdk"
}
$SdkPrefix = [System.IO.Path]::GetFullPath($SdkPrefix)

$toolsDir = Join-Path $SdkPrefix "tools\winflexbison-$Version"
$binDir = Join-Path $SdkPrefix "bin"
$archiveDir = Join-Path $WorkspaceRoot "downloads"
$archive = Join-Path $archiveDir "win_flex_bison-$Version.zip"
$url = "https://github.com/lexxmark/winflexbison/releases/download/v$Version/win_flex_bison-$Version.zip"

Write-Host "Scholia SDK winflexbison install" -ForegroundColor Green
Write-Host "Version     : $Version"
Write-Host "SdkPrefix   : $SdkPrefix"
Write-Host "Workspace   : $WorkspaceRoot"
Write-Host "URL         : $url"

New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
New-Item -ItemType Directory -Force -Path $binDir | Out-Null

if (!(Test-Path -LiteralPath $archive)) {
    Write-Host "Downloading: $archive"
    Invoke-WebRequest -Uri $url -OutFile $archive
}

if (Test-Path -LiteralPath $toolsDir) {
    Remove-Item -LiteralPath $toolsDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $toolsDir -Force

$winFlex = Get-ChildItem -LiteralPath $toolsDir -Recurse -Filter win_flex.exe | Select-Object -First 1
$winBison = Get-ChildItem -LiteralPath $toolsDir -Recurse -Filter win_bison.exe | Select-Object -First 1
$toolData = Get-ChildItem -LiteralPath $toolsDir -Recurse -Directory -Filter data | Select-Object -First 1
if (!$winFlex -or !$winBison -or !$toolData) {
    throw "Downloaded winflexbison archive did not contain win_flex.exe, win_bison.exe, and data resources."
}

Copy-Item -LiteralPath $winFlex.FullName -Destination (Join-Path $binDir "win_flex.exe") -Force
Copy-Item -LiteralPath $winBison.FullName -Destination (Join-Path $binDir "win_bison.exe") -Force
Copy-Item -LiteralPath $winFlex.FullName -Destination (Join-Path $binDir "flex.exe") -Force
Copy-Item -LiteralPath $winBison.FullName -Destination (Join-Path $binDir "bison.exe") -Force
$dataDir = Join-Path $binDir "data"
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
Get-ChildItem -LiteralPath $toolData.FullName | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $dataDir -Recurse -Force
}

$manifest = [pscustomobject]@{
    Name = "winflexbison"
    Version = $Version
    Url = $url
    InstalledAt = (Get-Date -Format o)
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $SdkPrefix "scholia-sdk-winflexbison.json") -Encoding UTF8

Write-Host ""
Write-Host "Installed winflexbison into SDK bin:" -ForegroundColor Green
Write-Host "  $(Join-Path $binDir 'flex.exe')"
Write-Host "  $(Join-Path $binDir 'bison.exe')"
