[CmdletBinding()]
param(
    [string] $WorkspaceRoot = "",
    [string] $Proxy = "http://127.0.0.1:7890"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "Continue"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (!$WorkspaceRoot) {
    $WorkspaceRoot = Split-Path -Parent $repoRoot
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)

$logDir = Join-Path $WorkspaceRoot "build-logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $logDir "tinytex-install-$stamp.log"
$installer = Join-Path $WorkspaceRoot "install-bin-windows.ps1"
$tmpDir = Join-Path $WorkspaceRoot "tmp"
New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null

$env:HTTP_PROXY = $Proxy
$env:HTTPS_PROXY = $Proxy
$env:http_proxy = $Proxy
$env:https_proxy = $Proxy
$env:TINYTEX_DIR = $WorkspaceRoot
$env:TEMP = $tmpDir
$env:TMP = $tmpDir

Start-Transcript -Path $logPath -Force | Out-Null
try {
    Write-Host "TinyTeX install started at $(Get-Date -Format o)"
    Write-Host "Log: $logPath"
    Write-Host "Install dir: $env:TINYTEX_DIR"
    Write-Host "Proxy: $env:HTTPS_PROXY"
    Write-Host ""

    if (!(Test-Path -LiteralPath $installer)) {
        Write-Host "Downloading TinyTeX PowerShell installer..."
        Invoke-WebRequest `
            -Uri "https://tinytex.yihui.org/install-bin-windows.ps1" `
            -OutFile $installer `
            -Proxy $Proxy
    } else {
        Write-Host "Using existing installer: $installer"
    }

    Write-Host ""
    Write-Host "Running installer..."
    & powershell.exe -ExecutionPolicy Bypass -NoProfile -File $installer
    $exitCode = $LASTEXITCODE
    Write-Host ""
    Write-Host "Installer exited with code $exitCode at $(Get-Date -Format o)"
    exit $exitCode
}
finally {
    Stop-Transcript | Out-Null
}
