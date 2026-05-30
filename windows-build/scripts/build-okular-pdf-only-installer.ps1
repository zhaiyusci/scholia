[CmdletBinding()]
param(
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $StageRoot = "",
    [string] $OutputDir = "",
    [string] $Version = "",
    [string] $FileVersion = "",
    [string] $ISCC = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    [switch] $SkipStage
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$workspaceRoot = Split-Path -Parent $repoRoot
if (!$StageRoot) {
    $StageRoot = Join-Path $workspaceRoot "dist\okular-pdf-only\app"
}
if (!$OutputDir) {
    $OutputDir = Join-Path $workspaceRoot "dist"
}
$StageRoot = [System.IO.Path]::GetFullPath($StageRoot)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

$buildDate = Get-Date -Format "yyyyMMdd"
if (!$Version) {
    $Version = "26.07.70-patch$buildDate"
}
if (!$FileVersion) {
    $fileDate = Get-Date -Format "MMdd"
    $FileVersion = "26.7.70.$([int]$fileDate)"
}

if (!$SkipStage) {
    & (Join-Path $PSScriptRoot "prepare-okular-pdf-only-stage.ps1") -CraftRoot $CraftRoot -StageRoot $StageRoot
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (!(Test-Path -LiteralPath $ISCC)) {
    throw "Cannot find Inno Setup compiler: $ISCC"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$env:OKULAR_PDF_STAGE = $StageRoot
$env:OKULAR_PDF_OUTPUT = $OutputDir
$env:OKULAR_PDF_VERSION = $Version
$env:OKULAR_PDF_FILE_VERSION = $FileVersion

$iss = Join-Path (Split-Path -Parent $PSScriptRoot) "installer\okular-pdf-only.iss"
Write-Host "Building installer with Inno Setup:"
Write-Host "  Script: $iss"
Write-Host "  Stage: $StageRoot"
Write-Host "  Output: $OutputDir"
Write-Host "  Version: $Version"
Write-Host "  File version: $FileVersion"
Write-Host ""

& $ISCC $iss
exit $LASTEXITCODE
