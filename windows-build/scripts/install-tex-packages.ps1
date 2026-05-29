[CmdletBinding()]
param(
    [string] $WorkspaceRoot = "",
    [string] $Proxy = "http://127.0.0.1:7890",
    [string[]] $Packages = @("standalone", "amsmath", "physics", "mhchem", "varwidth", "mathtools")
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
$logPath = Join-Path $logDir "tex-packages-$stamp.log"

$texBin = Join-Path $WorkspaceRoot "TinyTeX\bin\windows"
$env:Path = "$texBin;$env:Path"
$env:HTTP_PROXY = $Proxy
$env:HTTPS_PROXY = $Proxy
$env:http_proxy = $Proxy
$env:https_proxy = $Proxy

Start-Transcript -Path $logPath -Force | Out-Null
try {
    Write-Host "TeX package install started at $(Get-Date -Format o)"
    Write-Host "Log: $logPath"
    Write-Host "TinyTeX: $texBin"
    Write-Host "Proxy: $env:HTTPS_PROXY"
    Write-Host "Packages: $($Packages -join ', ')"
    Write-Host ""

    & tlmgr install @Packages
    $exitCode = $LASTEXITCODE

    Write-Host ""
    Write-Host "tlmgr exited with code $exitCode at $(Get-Date -Format o)"
    exit $exitCode
}
finally {
    Stop-Transcript | Out-Null
}
