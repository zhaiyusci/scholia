[CmdletBinding()]
param(
    [string] $WorkspaceRoot = "",
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $OkularVersion = "local",
    [string] $PopplerVersion = "local",
    [string] $MicroTeXSrc = "",
    [switch] $PdfOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (!$WorkspaceRoot) {
    $WorkspaceRoot = Split-Path -Parent $repoRoot
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)

$logDir = Join-Path $WorkspaceRoot "build-logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $logDir "okular-build-$stamp.log"

Start-Transcript -Path $logPath -Force | Out-Null
try {
    Write-Host "Okular local build started at $(Get-Date -Format o)"
    Write-Host "Log: $logPath"
    Write-Host "Craft root: $CraftRoot"
    Write-Host "Okular option: kde/applications/okular.version=$OkularVersion"
    Write-Host "Poppler option: qt-libs/poppler.version=$PopplerVersion"
    Write-Host "PDF-only generator build: $PdfOnly"
    Write-Host "MicroTeX source: $MicroTeXSrc"
    Write-Host ""

    $craftArgs = @(
        "--options", "kde/applications/okular.version=$OkularVersion",
        "--options", "qt-libs/poppler.version=$PopplerVersion"
    )
    $okularCMakeArgs = @()
    if ($PdfOnly) {
        $okularCMakeArgs += "-DOKULAR_PDF_ONLY=ON"
    }
    if ($MicroTeXSrc) {
        $resolvedMicroTeXSrc = (Resolve-Path -LiteralPath $MicroTeXSrc).Path
        $resolvedMicroTeXSrc = $resolvedMicroTeXSrc -replace "\\", "/"
        $okularCMakeArgs += "-DOKULAR_ENABLE_MICROTEX=ON"
        $okularCMakeArgs += "-DMICROTEX_SRC=$resolvedMicroTeXSrc"
    }
    if ($okularCMakeArgs.Count -gt 0) {
        $craftArgs += @("--options", "kde/applications/okular.args=$($okularCMakeArgs -join ' ')")
    }
    $craftArgs += @("--ignoreInstalled", "kde/applications/okular")

    & (Join-Path $PSScriptRoot "craft.ps1") `
        @craftArgs

    $exitCode = $LASTEXITCODE
    Write-Host ""
    Write-Host "Craft exited with code $exitCode at $(Get-Date -Format o)"
    exit $exitCode
}
finally {
    Stop-Transcript | Out-Null
}
