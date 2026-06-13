[CmdletBinding()]
param(
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $OkularSource = "",
    [int] $Jobs = 4,
    [switch] $Clean,
    [switch] $PopplerIncrementalOnly,
    [switch] $NoInstall
)

$ErrorActionPreference = "Stop"

if (!$OkularSource) {
    $OkularSource = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
}

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$cmake = Join-Path $CraftRoot "dev-utils\bin\cmake.exe"
$ninja = Join-Path $CraftRoot "dev-utils\bin\ninja.exe"
$popplerSource = Join-Path $OkularSource "external\poppler"
$okularBuild = Join-Path $CraftRoot "build\kde\applications\okular\work\build"
$popplerBuild = Join-Path $CraftRoot "build\qt-libs\poppler\work\build"

function Invoke-VsCmd([string] $Command) {
    Write-Host ""
    Write-Host ">>> $Command" -ForegroundColor Cyan
    & cmd.exe /d /s /c "call ""$vcvars"" >nul && $Command"
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

function Reset-BuildDir([string] $Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $allowed = [System.IO.Path]::GetFullPath((Join-Path $CraftRoot "build"))
    if (!$full.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove build directory outside $allowed`: $full"
    }
    if (Test-Path -LiteralPath $full) {
        Write-Host "Removing build directory: $full" -ForegroundColor Yellow
        Remove-Item -LiteralPath $full -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $full | Out-Null
}

Write-Host "Scholia from-submodule rebuild" -ForegroundColor Green
Write-Host "CraftRoot     : $CraftRoot"
Write-Host "Okular source : $OkularSource"
Write-Host "Poppler source: $popplerSource"
Write-Host "Jobs          : $Jobs"
Write-Host "Clean         : $Clean"
Write-Host "Poppler incr. : $PopplerIncrementalOnly"
Write-Host "No install    : $NoInstall"

if (!(Test-Path -LiteralPath (Join-Path $OkularSource ".gitmodules"))) {
    throw "Okular source does not look like a submodule-enabled checkout: $OkularSource"
}
if (!(Test-Path -LiteralPath (Join-Path $popplerSource "CMakeLists.txt"))) {
    throw "Poppler submodule is missing or not initialized: $popplerSource"
}
if (!(Test-Path -LiteralPath (Join-Path $OkularSource "external\MicroTeX\CMakeLists.txt"))) {
    throw "MicroTeX submodule is missing or not initialized."
}
if (!(Test-Path -LiteralPath $vcvars)) {
    throw "Cannot find vcvars64.bat: $vcvars"
}

if ($Clean) {
    if ($PopplerIncrementalOnly) {
        throw "-Clean cannot be combined with -PopplerIncrementalOnly."
    }
    Reset-BuildDir $popplerBuild
    Reset-BuildDir $okularBuild
}

if ($PopplerIncrementalOnly) {
    Write-Host ""
    Write-Host "Poppler incremental mode: building exact DLL targets and copying runtime files only." -ForegroundColor Green
    & (Join-Path $PSScriptRoot "build-poppler-incremental.ps1") `
        -CraftRoot $CraftRoot `
        -OkularSource $OkularSource `
        -BuildDir $popplerBuild `
        -Jobs $Jobs `
        -NoInstall:$NoInstall
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "[1/4] Configure Poppler submodule" -ForegroundColor Green
Invoke-VsCmd """$cmake"" -S ""$popplerSource"" -B ""$popplerBuild"" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=C:/CraftRoot -DCMAKE_PREFIX_PATH=C:/CraftRoot -DENABLE_QT5=OFF -DENABLE_QT6=ON -DBUILD_QT6_TESTS=OFF -DENABLE_UTILS=OFF -DENABLE_CPP=ON"

Write-Host ""
Write-Host "[2/4] Build and install Poppler submodule" -ForegroundColor Green
Invoke-VsCmd """$ninja"" -C ""$popplerBuild"" -j$Jobs install"

Write-Host ""
Write-Host "[3/4] Configure Scholia with submodule MicroTeX and installed submodule Poppler" -ForegroundColor Green
Invoke-VsCmd """$cmake"" -S ""$OkularSource"" -B ""$okularBuild"" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=C:/CraftRoot -DCMAKE_PREFIX_PATH=C:/CraftRoot -DBUILD_TESTING=OFF -DOKULAR_PDF_ONLY=ON -DOKULAR_ENABLE_MICROTEX=ON -DCMAKE_DISABLE_FIND_PACKAGE_KF6DocTools=ON"

Write-Host ""
Write-Host "[4/4] Build and install Scholia" -ForegroundColor Green
Invoke-VsCmd """$ninja"" -C ""$okularBuild"" -j$Jobs install"

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "Scholia: $(Get-Item -LiteralPath (Join-Path $CraftRoot 'bin\scholia.exe') | Select-Object -ExpandProperty LastWriteTime)"
Write-Host "PDF gen: $(Get-Item -LiteralPath (Join-Path $CraftRoot 'plugins\okular_generators\okularGenerator_poppler.dll') | Select-Object -ExpandProperty LastWriteTime)"
