[CmdletBinding()]
param(
    [string] $StageRoot = "",
    [string] $PdfPath = "",
    [int] $WaitSeconds = 15
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$workspaceRoot = Split-Path -Parent $repoRoot
$windowsBuildRoot = Join-Path $workspaceRoot "windows_build"
if (!$StageRoot) {
    $StageRoot = Join-Path $windowsBuildRoot "dist\okular-pdf-only\app"
}
$StageRoot = [System.IO.Path]::GetFullPath($StageRoot)

if (!$PdfPath) {
    throw "Pass -PdfPath with a local PDF to smoke test."
}

if (!(Test-Path -LiteralPath $PdfPath)) {
    throw "Missing PDF smoke-test file: $PdfPath"
}
$PdfPath = [System.IO.Path]::GetFullPath($PdfPath)

$okularExe = Join-Path $StageRoot "bin\okular.exe"
if (!(Test-Path -LiteralPath $okularExe)) {
    throw "Missing staged Okular executable: $okularExe"
}

Get-Process -Name okular -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "$StageRoot*" } |
    Stop-Process -Force

$oldPath = $env:PATH
try {
    $env:PATH = "$StageRoot\bin;$env:SystemRoot\System32;$env:SystemRoot"
    Start-Process -FilePath $okularExe -ArgumentList "`"$PdfPath`"" -WorkingDirectory (Join-Path $StageRoot "bin")
    Start-Sleep -Seconds $WaitSeconds

    $process = Get-Process -Name okular -ErrorAction Stop |
        Where-Object { $_.Path -like "$StageRoot*" } |
        Select-Object -First 1

    if (!$process) {
        throw "Staged Okular is not running."
    }

    $modules = $process.Modules | ForEach-Object FileName | Where-Object { $_ }
    $craftRootModules = $modules | Where-Object { $_ -like "C:\CraftRoot\*" }
    $pdfModules = $modules | Where-Object { $_ -match "okularGenerator|poppler|okularpart" } | Sort-Object

    [pscustomobject]@{
        ProcessId = $process.Id
        Responding = $process.Responding
        MainWindowTitle = $process.MainWindowTitle
        CraftRootModuleCount = @($craftRootModules).Count
        PdfModules = $pdfModules
    } | Format-List

    if ($craftRootModules) {
        Write-Warning "Staged Okular loaded modules from CraftRoot:"
        $craftRootModules | Sort-Object | ForEach-Object { Write-Warning "  $_" }
        exit 2
    }

    $requiredPdfModules = @(
        Join-Path $StageRoot "bin\poppler.dll"
        Join-Path $StageRoot "bin\poppler-qt6.dll"
        Join-Path $StageRoot "plugins\kf6\parts\okularpart.dll"
    )
    $missingPdfModules = $requiredPdfModules | Where-Object { $pdfModules -notcontains $_ }
    if ($missingPdfModules) {
        Write-Warning "Required staged PDF modules were not loaded:"
        $missingPdfModules | ForEach-Object { Write-Warning "  $_" }
        exit 3
    }

    $pdfGenerator = Join-Path $StageRoot "plugins\okular_generators\okularGenerator_poppler.dll"
    if ($pdfModules -notcontains $pdfGenerator) {
        Write-Warning "PDF generator module was not visible in the process module list. If the PDF opened successfully, this can be a loader-timing false negative."
    }

    exit 0
}
finally {
    $env:PATH = $oldPath
}
