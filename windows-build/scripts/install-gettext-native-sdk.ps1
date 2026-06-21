[CmdletBinding()]
param(
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $Version = "1.0",
    [string] $IconvVersion = "1.19"
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

$releaseTag = "v$Version-v$IconvVersion"
$archiveName = "gettext$Version-iconv$IconvVersion-static-64.zip"
$archiveSha256 = "2B999BC37B56FA052FF98578E9CB188152009F634E3A66AA8FF263A016E9C466"
$url = "https://github.com/mlocati/gettext-iconv-windows/releases/download/$releaseTag/$archiveName"
$downloadDir = Join-Path $WorkspaceRoot "downloads"
$archive = Join-Path $downloadDir $archiveName
$toolsRoot = Join-Path $SdkPrefix "tools"
$installDir = Join-Path $toolsRoot "gettext-native"

function Assert-PathInside([string] $Path, [string] $AllowedRoot) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $allowed = [System.IO.Path]::GetFullPath($AllowedRoot)
    if (!$full.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify path outside $allowed`: $full"
    }
    return $full
}

function Test-FileSha256([string] $Path, [string] $ExpectedHash) {
    if (!(Test-Path -LiteralPath $Path)) {
        return $false
    }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    return $actual.Equals($ExpectedHash, [System.StringComparison]::OrdinalIgnoreCase)
}

Write-Host "Scholia SDK native gettext install" -ForegroundColor Green
Write-Host "Version     : gettext $Version, iconv $IconvVersion"
Write-Host "SdkPrefix   : $SdkPrefix"
Write-Host "Workspace   : $WorkspaceRoot"
Write-Host "URL         : $url"

New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null

if (!(Test-FileSha256 $archive $archiveSha256)) {
    if (Test-Path -LiteralPath $archive) {
        Write-Host "Removing cached archive with unexpected hash: $archive" -ForegroundColor Yellow
        Remove-Item -LiteralPath $archive -Force
    }
    Write-Host "Downloading: $archive"
    Invoke-WebRequest -Uri $url -OutFile $archive
}

if (!(Test-FileSha256 $archive $archiveSha256)) {
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    throw "Downloaded archive SHA256 mismatch. Expected $archiveSha256, got $actual."
}

$installDir = Assert-PathInside $installDir $toolsRoot
if (Test-Path -LiteralPath $installDir) {
    Remove-Item -LiteralPath $installDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $installDir | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $installDir -Force

$msgfmt = Join-Path $installDir "bin\msgfmt.exe"
if (!(Test-Path -LiteralPath $msgfmt)) {
    throw "Downloaded gettext archive did not contain bin\msgfmt.exe."
}

$versionOutput = & $msgfmt --version 2>&1
if ($LASTEXITCODE -ne 0 -or (($versionOutput -join "`n") -notmatch "GNU gettext-tools")) {
    throw "Installed msgfmt did not look like GNU gettext-tools: $msgfmt"
}

$manifest = [pscustomobject]@{
    Name = "gettext-iconv-windows"
    Version = $Version
    IconvVersion = $IconvVersion
    ReleaseTag = $releaseTag
    Archive = $archiveName
    Sha256 = $archiveSha256
    Url = $url
    InstalledAt = (Get-Date -Format o)
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $SdkPrefix "scholia-sdk-gettext-native.json") -Encoding UTF8

Write-Host ""
Write-Host "Installed native gettext into SDK:" -ForegroundColor Green
Write-Host "  $msgfmt"
Write-Host "  $($versionOutput[0])"
