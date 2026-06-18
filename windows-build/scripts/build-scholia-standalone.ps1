[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $InstallPrefix = "",
    [string] $BuildType = "RelWithDebInfo",
    [int] $Jobs = 8,
    [string] $CMake = "",
    [string] $Ninja = "",
    [string] $VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    [switch] $Clean,
    [switch] $EnableMicroTeX
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
if (!$InstallPrefix) {
    $InstallPrefix = Join-Path $WorkspaceRoot "install\scholia"
}
$InstallPrefix = [System.IO.Path]::GetFullPath($InstallPrefix)

function Resolve-CommandPath([string] $Name, [string[]] $Candidates) {
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    throw "Cannot find $Name. Pass an explicit path with the matching parameter."
}

function Find-QtPrefix([string] $RequestedPrefix) {
    $candidates = @()
    if ($RequestedPrefix) {
        $candidates += $RequestedPrefix
    }
    if ($env:QTDIR) {
        $candidates += $env:QTDIR
    }
    if (Test-Path -LiteralPath "C:\Qt") {
        $candidates += Get-ChildItem -LiteralPath "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName "msvc2022_64" }
    }
    foreach ($candidate in ($candidates | Where-Object { $_ } | Select-Object -Unique)) {
        $full = [System.IO.Path]::GetFullPath($candidate)
        if (Test-Path -LiteralPath (Join-Path $full "lib\cmake\Qt6\Qt6Config.cmake")) {
            return $full
        }
    }
    throw "Cannot find a Qt MSVC prefix. Pass -QtPrefix, for example C:\Qt\6.11.1\msvc2022_64."
}

function Invoke-VsCmd([string] $Command) {
    Write-Host ""
    Write-Host ">>> $Command" -ForegroundColor Cyan
    $qtBin = Join-Path $script:QtPrefix "bin"
    $sdkBin = Join-Path $script:SdkPrefix "bin"
    & cmd.exe /d /s /c "set ""PATH=$qtBin;$sdkBin;%PATH%"" && call ""$VcVars"" >nul && $Command"
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

function Reset-Directory([string] $Path, [string] $AllowedRoot) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $allowed = [System.IO.Path]::GetFullPath($AllowedRoot)
    if (!$full.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove directory outside $allowed`: $full"
    }
    if (Test-Path -LiteralPath $full) {
        Write-Host "Removing: $full" -ForegroundColor Yellow
        Remove-Item -LiteralPath $full -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $full | Out-Null
}

$QtPrefix = Find-QtPrefix $QtPrefix
$CMake = Resolve-CommandPath "cmake" @($CMake, (Join-Path $QtPrefix "..\..\Tools\CMake_64\bin\cmake.exe"), "C:\Program Files\CMake\bin\cmake.exe")
$Ninja = Resolve-CommandPath "ninja" @($Ninja, (Join-Path $QtPrefix "..\..\Tools\Ninja\ninja.exe"))
if (!(Test-Path -LiteralPath $VcVars)) {
    throw "Cannot find vcvars64.bat: $VcVars"
}
foreach ($required in @(
    "share\ECM\cmake\ECMConfig.cmake",
    "lib\cmake\KF6Parts\KF6PartsConfig.cmake",
    "lib\poppler.lib",
    "lib\poppler-qt6.lib"
)) {
    if (!(Test-Path -LiteralPath (Join-Path $SdkPrefix $required))) {
        throw "Standalone SDK is missing $required in $SdkPrefix."
    }
}

$buildRoot = Join-Path $WorkspaceRoot "build"
$buildDir = [System.IO.Path]::GetFullPath((Join-Path $buildRoot "scholia-standalone"))
if ($Clean) {
    Reset-Directory $buildDir $buildRoot
} else {
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
}

$forceNotRequired = @(
    "KF6Purpose",
    "Qt6TextToSpeech",
    "Phonon4Qt6",
    "Freetype",
    "TIFF",
    "LibSpectre",
    "KExiv2Qt6",
    "DjVuLibre",
    "EPub",
    "Discount"
) -join ";"

Write-Host "Scholia standalone build" -ForegroundColor Green
Write-Host "QtPrefix    : $QtPrefix"
Write-Host "SdkPrefix   : $SdkPrefix"
Write-Host "BuildDir    : $buildDir"
Write-Host "Install     : $InstallPrefix"
Write-Host "MicroTeX    : $EnableMicroTeX"

$configureArgs = @(
    """$CMake""",
    "-S", """$repoRoot""",
    "-B", """$buildDir""",
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=""$Ninja""",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_INSTALL_PREFIX=""$InstallPrefix""",
    "-DCMAKE_PREFIX_PATH=""$QtPrefix;$SdkPrefix""",
    "-DBUILD_TESTING=OFF",
    "-DOKULAR_PDF_ONLY=ON",
    "-DOKULAR_ENABLE_MICROTEX=$(if ($EnableMicroTeX) { 'ON' } else { 'OFF' })",
    "-DCMAKE_DISABLE_FIND_PACKAGE_KF6DocTools=ON",
    "-DKDE_INSTALL_PLUGINDIR=plugins",
    "-DFORCE_NOT_REQUIRED_DEPENDENCIES=""$forceNotRequired"""
)
Invoke-VsCmd ($configureArgs -join " ")

$build = """$CMake"" --build ""$buildDir"" --target install --parallel $Jobs"
Invoke-VsCmd $build

Write-Host ""
Write-Host "Scholia standalone install complete." -ForegroundColor Green
