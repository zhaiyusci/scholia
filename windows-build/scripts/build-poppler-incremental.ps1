[CmdletBinding()]
param(
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $OkularSource = "",
    [string] $BuildDir = "",
    [int] $Jobs = 0,
    [switch] $NoInstall
)

$ErrorActionPreference = "Stop"

if (!$OkularSource) {
    $OkularSource = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
}
if (!$BuildDir) {
    $BuildDir = Join-Path $CraftRoot "build\qt-libs\poppler\work\build"
}

$OkularSource = [System.IO.Path]::GetFullPath($OkularSource)
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$workspaceRoot = Split-Path -Parent $OkularSource
$logRoot = Join-Path $workspaceRoot "windows_build"
$logDir = Join-Path $logRoot "build-logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $logDir "poppler-incremental-$stamp.log"

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$ninja = Join-Path $CraftRoot "dev-utils\bin\ninja.exe"

function Copy-IfExists([string] $From, [string] $To) {
    if (Test-Path -LiteralPath $From) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $To) | Out-Null
        Copy-Item -LiteralPath $From -Destination $To -Force
        Write-Host "Install: $To"
    }
}

Start-Transcript -Path $logPath -Force | Out-Null
try {
    Write-Host "Poppler incremental Windows build started at $(Get-Date -Format o)"
    Write-Host "Log: $logPath"
    Write-Host "Craft root: $CraftRoot"
    Write-Host "Okular source: $OkularSource"
    Write-Host "Build dir: $BuildDir"
    Write-Host "Install runtime: $(!$NoInstall)"
    Write-Host ""

    if (!(Test-Path -LiteralPath $vcvars)) {
        throw "Cannot find vcvars64.bat: $vcvars"
    }
    if (!(Test-Path -LiteralPath $ninja)) {
        throw "Cannot find ninja: $ninja"
    }
    if (!(Test-Path -LiteralPath (Join-Path $BuildDir "build.ninja"))) {
        throw "Missing Poppler Ninja build directory: $BuildDir. Run the full Windows build once first."
    }

    $targets = @("poppler.dll", "qt6\src\poppler-qt6.dll")
    Write-Host "Ninja targets:"
    $targets | ForEach-Object { Write-Host "  $_" }
    Write-Host ""

    $jobArg = ""
    if ($Jobs -gt 0) {
        $jobArg = " -j$Jobs"
    }
    $targetArgs = $targets -join " "
    $command = """$ninja"" -C ""$BuildDir""$jobArg $targetArgs"
    & cmd.exe /d /s /c "call ""$vcvars"" >nul && $command"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    if (!$NoInstall) {
        $runningScholia = Get-Process scholia -ErrorAction SilentlyContinue
        if ($runningScholia) {
            throw "Poppler DLLs were rebuilt, but scholia.exe is running. Close it, then rerun this script to install updated DLLs into $CraftRoot\bin."
        }

        Write-Host ""
        Copy-IfExists (Join-Path $BuildDir "poppler.dll") (Join-Path $CraftRoot "bin\poppler.dll")
        Copy-IfExists (Join-Path $BuildDir "poppler.pdb") (Join-Path $CraftRoot "bin\poppler.pdb")
        Copy-IfExists (Join-Path $BuildDir "poppler.lib") (Join-Path $CraftRoot "lib\poppler.lib")
        Copy-IfExists (Join-Path $BuildDir "qt6\src\poppler-qt6.dll") (Join-Path $CraftRoot "bin\poppler-qt6.dll")
        Copy-IfExists (Join-Path $BuildDir "qt6\src\poppler-qt6.pdb") (Join-Path $CraftRoot "bin\poppler-qt6.pdb")
        Copy-IfExists (Join-Path $BuildDir "qt6\src\poppler-qt6.lib") (Join-Path $CraftRoot "lib\poppler-qt6.lib")
    }

    Write-Host ""
    if ($NoInstall) {
        Write-Host "Current Poppler runtime timestamps:"
    } else {
        Write-Host "Installed Poppler runtime timestamps:"
    }
    Get-Item -LiteralPath (Join-Path $CraftRoot "bin\poppler.dll"), (Join-Path $CraftRoot "bin\poppler-qt6.dll") |
        Select-Object FullName, Length, LastWriteTime |
        Format-Table -AutoSize
    Write-Host "Poppler incremental Windows build finished at $(Get-Date -Format o)"
    exit 0
}
finally {
    Stop-Transcript | Out-Null
}
