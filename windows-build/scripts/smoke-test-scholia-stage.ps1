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
    $StageRoot = Join-Path $windowsBuildRoot "dist\scholia-pdf\app"
}
$StageRoot = [System.IO.Path]::GetFullPath($StageRoot)

if (!$PdfPath) {
    throw "Pass -PdfPath with a local PDF to smoke test."
}

if (!(Test-Path -LiteralPath $PdfPath)) {
    throw "Missing PDF smoke-test file: $PdfPath"
}
$PdfPath = [System.IO.Path]::GetFullPath($PdfPath)

$scholiaExe = Join-Path $StageRoot "bin\scholia.exe"
if (!(Test-Path -LiteralPath $scholiaExe)) {
    throw "Missing staged Scholia executable: $scholiaExe"
}

Get-Process -Name scholia -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "$StageRoot*" } |
    Stop-Process -Force

$oldPath = $env:PATH
$process = $null
try {
    $env:PATH = "$StageRoot\bin;$env:SystemRoot\System32;$env:SystemRoot"
    Start-Process -FilePath $scholiaExe -ArgumentList "`"$PdfPath`"" -WorkingDirectory (Join-Path $StageRoot "bin") -WindowStyle Minimized
    Start-Sleep -Seconds $WaitSeconds

    $process = Get-Process -Name scholia -ErrorAction Stop |
        Where-Object { $_.Path -like "$StageRoot*" } |
        Select-Object -First 1

    if (!$process) {
        throw "Staged Scholia is not running."
    }

    $modules = $process.Modules | ForEach-Object FileName | Where-Object { $_ }
    $unexpectedWorkspaceModules = $modules | Where-Object {
        $_ -like "$workspaceRoot\*" -and $_ -notlike "$StageRoot\*"
    }
    $pdfModules = $modules | Where-Object { $_ -match "okularGenerator|poppler|okularpart" } | Sort-Object

    [pscustomobject]@{
        ProcessId = $process.Id
        Responding = $process.Responding
        MainWindowTitle = $process.MainWindowTitle
        UnexpectedWorkspaceModuleCount = @($unexpectedWorkspaceModules).Count
        PdfModules = $pdfModules
    } | Format-List

    if ($unexpectedWorkspaceModules) {
        Write-Warning "Staged Scholia loaded project-local modules from outside the stage:"
        $unexpectedWorkspaceModules | Sort-Object | ForEach-Object { Write-Warning "  $_" }
        exit 2
    }

    $requiredPdfModules = @(
        Join-Path $StageRoot "bin\poppler.dll"
        Join-Path $StageRoot "bin\poppler-qt6.dll"
        Join-Path $StageRoot "bin\plugins\kf6\parts\okularpart.dll"
    )
    $missingPdfModules = $requiredPdfModules | Where-Object { $pdfModules -notcontains $_ }
    if ($missingPdfModules) {
        Write-Warning "Required staged PDF modules were not loaded:"
        $missingPdfModules | ForEach-Object { Write-Warning "  $_" }
        exit 3
    }

    $pdfGenerator = Join-Path $StageRoot "bin\plugins\okular_generators\okularGenerator_poppler.dll"
    if ($pdfModules -notcontains $pdfGenerator) {
        Write-Warning "PDF generator module was not visible in the process module list. If the PDF opened successfully, this can be a loader-timing false negative."
    }

    exit 0
}
finally {
    $env:PATH = $oldPath
    if ($process) {
        $process.CloseMainWindow() | Out-Null
        Start-Sleep -Seconds 2
        if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) {
            Stop-Process -Id $process.Id -Force
        }
    }
}
