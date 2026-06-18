[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $PopplerSource = "",
    [string] $BuildType = "RelWithDebInfo",
    [int] $Jobs = 8,
    [string] $CMake = "",
    [string] $Ninja = "",
    [string] $VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    [switch] $Clean
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
if (!$PopplerSource) {
    $PopplerSource = Join-Path $repoRoot "external\poppler"
}
$PopplerSource = [System.IO.Path]::GetFullPath($PopplerSource)

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
if (!(Test-Path -LiteralPath (Join-Path $SdkPrefix "lib\zlib.lib"))) {
    throw "zlib is not installed in $SdkPrefix. Run build-zlib-sdk.ps1 first."
}
if (!(Test-Path -LiteralPath (Join-Path $SdkPrefix "lib\freetype.lib"))) {
    throw "freetype is not installed in $SdkPrefix. Run build-freetype-sdk.ps1 first."
}
if (!(Test-Path -LiteralPath (Join-Path $PopplerSource "CMakeLists.txt"))) {
    throw "Cannot find Poppler source: $PopplerSource"
}

$buildRoot = Join-Path $WorkspaceRoot "build\thirdparty"
$popplerBuild = [System.IO.Path]::GetFullPath((Join-Path $buildRoot "poppler-custom"))

Write-Host "Scholia SDK Poppler build" -ForegroundColor Green
Write-Host "Source    : $PopplerSource"
Write-Host "SdkPrefix : $SdkPrefix"
Write-Host "QtPrefix  : $QtPrefix"

if ($Clean) {
    Reset-Directory $popplerBuild $buildRoot
} else {
    New-Item -ItemType Directory -Force -Path $popplerBuild | Out-Null
}

$prefixPath = "$QtPrefix;$SdkPrefix"
$configure = @(
    """$CMake""",
    "-S", """$PopplerSource""",
    "-B", """$popplerBuild""",
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=""$Ninja""",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_INSTALL_PREFIX=""$SdkPrefix""",
    "-DCMAKE_PREFIX_PATH=""$prefixPath""",
    "-DBUILD_SHARED_LIBS=ON",
    "-DBUILD_TESTING=OFF",
    "-DBUILD_QT6_TESTS=OFF",
    "-DBUILD_QT5_TESTS=OFF",
    "-DBUILD_CPP_TESTS=OFF",
    "-DBUILD_GTK_TESTS=OFF",
    "-DBUILD_MANUAL_TESTS=OFF",
    "-DENABLE_UTILS=OFF",
    "-DENABLE_CPP=OFF",
    "-DENABLE_GLIB=OFF",
    "-DENABLE_GOBJECT_INTROSPECTION=OFF",
    "-DENABLE_GTK_DOC=OFF",
    "-DENABLE_QT5=OFF",
    "-DENABLE_QT6=ON",
    "-DENABLE_BOOST=OFF",
    "-DENABLE_LIBOPENJPEG=UnmaintainedWillBeRemovedInJuly2026",
    "-DENABLE_DCTDECODER=UnmaintainedWillBeRemovedInJuly2026",
    "-DENABLE_LCMS=OFF",
    "-DENABLE_LIBCURL=OFF",
    "-DENABLE_LIBTIFF=OFF",
    "-DENABLE_NSS3=OFF",
    "-DENABLE_GPGME=OFF",
    "-DENABLE_ZLIB_UNCOMPRESS=ON",
    "-DRUN_GPERF_IF_PRESENT=OFF",
    "-DFONT_CONFIGURATION=win32"
) -join " "
Invoke-VsCmd $configure

$build = """$CMake"" --build ""$popplerBuild"" --target install --parallel $Jobs"
Invoke-VsCmd $build

Write-Host ""
Write-Host "custom Poppler installed into SDK." -ForegroundColor Green
