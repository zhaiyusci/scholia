[CmdletBinding()]
param(
    [string] $WorkspaceRoot = "",
    [string] $LogRoot = "",
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $CraftOkularSource = "",
    [string] $BuildDir = "C:\CraftRoot\build\kde\applications\okular\work\build",
    [string[]] $SourceFiles = @(
        "part\latexnoteutils.cpp",
        "part\annotationpopup.cpp",
        "part\pageviewmouseannotation.cpp",
        "part\pageviewannotator.cpp"
    ),
    [switch] $NoInstall
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (!$WorkspaceRoot) {
    $WorkspaceRoot = Split-Path -Parent $repoRoot
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)
if (!$LogRoot) {
    $LogRoot = Join-Path (Split-Path -Parent $repoRoot) "windows_build"
}
$LogRoot = [System.IO.Path]::GetFullPath($LogRoot)

$localOkular = Join-Path $WorkspaceRoot "okular"
if (!$CraftOkularSource) {
    $CraftOkularSource = $localOkular
}
$logDir = Join-Path $LogRoot "build-logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $logDir "okularpart-incremental-$stamp.log"

function Convert-ToObjTarget([string] $sourceFile) {
    $normalized = $sourceFile -replace "\\", "/"
    return "CMakeFiles/okularpart.dir/$normalized.obj"
}

Start-Transcript -Path $logPath -Force | Out-Null
try {
    Write-Host "Okular part incremental build started at $(Get-Date -Format o)"
    Write-Host "Log: $logPath"
    Write-Host "Local Okular: $localOkular"
    Write-Host "Craft Okular source: $CraftOkularSource"
    Write-Host "Log root: $LogRoot"
    Write-Host "Build dir: $BuildDir"
    Write-Host ""

    foreach ($sourceFile in $SourceFiles) {
        $from = Join-Path $localOkular $sourceFile
        $to = Join-Path $CraftOkularSource $sourceFile
        if (!(Test-Path -LiteralPath $from)) {
            throw "Missing source file: $from"
        }
        if ([System.IO.Path]::GetFullPath($from) -eq [System.IO.Path]::GetFullPath($to)) {
            Write-Host "Sync skipped, source already in Craft tree: $sourceFile"
        } else {
            Write-Host "Sync: $sourceFile"
            Copy-Item -LiteralPath $from -Destination $to -Force
        }
    }

    $mingwPath = "C:\Users\Yu Zhai\mingw64\bin"
    $env:Path = (($env:Path -split ";") | Where-Object { $_ -and ($_ -ne $mingwPath) }) -join ";"

    & (Join-Path $CraftRoot "craft\craftenv.ps1")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Set-Location $BuildDir

    $targets = @()
    foreach ($sourceFile in $SourceFiles) {
        if ($sourceFile.ToLowerInvariant().EndsWith(".cpp")) {
            $targets += Convert-ToObjTarget $sourceFile
        }
    }
    $targets += "lib/okularpart.lib"
    $targets += "bin/okularpart.dll"

    Write-Host ""
    Write-Host "Ninja targets:"
    $targets | ForEach-Object { Write-Host "  $_" }
    Write-Host ""

    & (Join-Path $CraftRoot "dev-utils\bin\ninja.exe") @targets
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        exit $exitCode
    }

    if (!$NoInstall) {
        $builtDll = Join-Path $BuildDir "bin\okularpart.dll"
        $builtPdb = Join-Path $BuildDir "bin\okularpart.pdb"
        $installedDll = Join-Path $CraftRoot "plugins\kf6\parts\okularpart.dll"
        $imageDir = Join-Path $CraftRoot "build\kde\applications\okular\image-RelWithDebInfo-local\plugins\kf6\parts"

        Write-Host ""
        Write-Host "Install: $installedDll"
        Copy-Item -LiteralPath $builtDll -Destination $installedDll -Force

        if (Test-Path -LiteralPath $imageDir) {
            Write-Host "Install image copy: $imageDir"
            Copy-Item -LiteralPath $builtDll -Destination (Join-Path $imageDir "okularpart.dll") -Force
            if (Test-Path -LiteralPath $builtPdb) {
                Copy-Item -LiteralPath $builtPdb -Destination (Join-Path $imageDir "okularpart.pdb") -Force
            }
        }
    }

    Write-Host ""
    Write-Host "Incremental build finished at $(Get-Date -Format o)"
    exit 0
}
finally {
    Stop-Transcript | Out-Null
}
