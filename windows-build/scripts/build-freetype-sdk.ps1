[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $BuildType = "RelWithDebInfo",
    [int] $Jobs = 8,
    [string] $FreetypeVersion = "2.13.3",
    [string] $FreetypeGitRef = "",
    [string] $FreetypeGitUrl = "https://gitlab.freedesktop.org/freetype/freetype.git",
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

function Ensure-GitCheckout([string] $Url, [string] $Ref, [string] $Destination) {
    if (Test-Path -LiteralPath (Join-Path $Destination ".git")) {
        Write-Host "Using existing checkout: $Destination"
        Push-Location $Destination
        try {
            & git fetch --tags origin
            if ($LASTEXITCODE -ne 0) {
                throw "git fetch failed in $Destination"
            }
            & git checkout $Ref
            if ($LASTEXITCODE -ne 0) {
                throw "git checkout $Ref failed in $Destination"
            }
        } finally {
            Pop-Location
        }
        return
    }

    if (Test-Path -LiteralPath $Destination) {
        throw "Source path exists but is not a git checkout: $Destination"
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
    Write-Host "Cloning freetype: $Url -> $Destination"
    & git clone $Url $Destination
    if ($LASTEXITCODE -ne 0) {
        throw "git clone failed for $Url"
    }
    Push-Location $Destination
    try {
        & git checkout $Ref
        if ($LASTEXITCODE -ne 0) {
            throw "git checkout $Ref failed in $Destination"
        }
    } finally {
        Pop-Location
    }
}

$QtPrefix = Find-QtPrefix $QtPrefix
$CMake = Resolve-CommandPath "cmake" @($CMake, (Join-Path $QtPrefix "..\..\Tools\CMake_64\bin\cmake.exe"), "C:\Program Files\CMake\bin\cmake.exe")
$Ninja = Resolve-CommandPath "ninja" @($Ninja, (Join-Path $QtPrefix "..\..\Tools\Ninja\ninja.exe"))
if (!(Test-Path -LiteralPath $VcVars)) {
    throw "Cannot find vcvars64.bat: $VcVars"
}
if (!$FreetypeGitRef) {
    $FreetypeGitRef = "VER-$($FreetypeVersion.Replace('.', '-'))"
}

$sourceRoot = Join-Path $WorkspaceRoot "sources"
$buildRoot = Join-Path $WorkspaceRoot "build\thirdparty"
$freetypeSource = [System.IO.Path]::GetFullPath((Join-Path $sourceRoot "freetype"))
$freetypeBuild = [System.IO.Path]::GetFullPath((Join-Path $buildRoot "freetype"))

Write-Host "Scholia SDK freetype build" -ForegroundColor Green
Write-Host "Git ref   : $FreetypeGitRef"
Write-Host "SdkPrefix : $SdkPrefix"
Write-Host "QtPrefix  : $QtPrefix"

Ensure-GitCheckout $FreetypeGitUrl $FreetypeGitRef $freetypeSource
if ($Clean) {
    Reset-Directory $freetypeBuild $buildRoot
} else {
    New-Item -ItemType Directory -Force -Path $freetypeBuild | Out-Null
}

$prefixPath = "$QtPrefix;$SdkPrefix"
$configure = @(
    """$CMake""",
    "-S", """$freetypeSource""",
    "-B", """$freetypeBuild""",
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=""$Ninja""",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_INSTALL_PREFIX=""$SdkPrefix""",
    "-DCMAKE_PREFIX_PATH=""$prefixPath""",
    "-DBUILD_SHARED_LIBS=ON",
    "-DFT_DISABLE_BROTLI=ON",
    "-DFT_DISABLE_BZIP2=ON",
    "-DFT_DISABLE_HARFBUZZ=ON",
    "-DFT_DISABLE_PNG=ON",
    "-DFT_DISABLE_ZLIB=ON"
) -join " "
Invoke-VsCmd $configure

$build = """$CMake"" --build ""$freetypeBuild"" --target install --parallel $Jobs"
Invoke-VsCmd $build

Write-Host ""
Write-Host "freetype installed into SDK." -ForegroundColor Green
