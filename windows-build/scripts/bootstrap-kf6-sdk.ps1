[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $BuildType = "RelWithDebInfo",
    [int] $Jobs = 8,
    [string] $EcmSource = "",
    [string] $EcmGitUrl = "https://invent.kde.org/frameworks/extra-cmake-modules.git",
    [string] $Kf6Version = "6.27.0",
    [string] $EcmGitRef = "",
    [string] $CMake = "",
    [string] $Ninja = "",
    [string] $VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    [switch] $Clean,
    [switch] $SkipEcm
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
    if ($env:Qt6_DIR) {
        $qt6Dir = [System.IO.Path]::GetFullPath($env:Qt6_DIR)
        $qtPrefixFromConfig = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qt6Dir))
        $candidates += $qtPrefixFromConfig
    }
    if (Test-Path -LiteralPath "C:\Qt") {
        $candidates += Get-ChildItem -LiteralPath "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            ForEach-Object {
                Join-Path $_.FullName "msvc2022_64"
            }
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
    & cmd.exe /d /s /c "call ""$VcVars"" >nul && $Command"
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
        throw "ECM source path exists but is not a git checkout: $Destination"
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
    Write-Host "Cloning ECM: $Url -> $Destination"
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
$CMake = Resolve-CommandPath "cmake" @(
    $CMake,
    (Join-Path $QtPrefix "..\..\Tools\CMake_64\bin\cmake.exe"),
    "C:\Program Files\CMake\bin\cmake.exe"
)
$Ninja = Resolve-CommandPath "ninja" @(
    $Ninja,
    (Join-Path $QtPrefix "..\..\Tools\Ninja\ninja.exe")
)

if (!(Test-Path -LiteralPath $VcVars)) {
    throw "Cannot find vcvars64.bat: $VcVars"
}

$sourceRoot = Join-Path $WorkspaceRoot "sources"
$buildRoot = Join-Path $WorkspaceRoot "build\kf6-sdk"
if (!$EcmSource) {
    $EcmSource = Join-Path $sourceRoot "extra-cmake-modules"
}
if (!$EcmGitRef) {
    $EcmGitRef = "v$Kf6Version"
}
$EcmSource = [System.IO.Path]::GetFullPath($EcmSource)
$ecmBuild = Join-Path $buildRoot "extra-cmake-modules"

Write-Host "Scholia KF6 SDK bootstrap" -ForegroundColor Green
Write-Host "QtPrefix    : $QtPrefix"
Write-Host "SdkPrefix   : $SdkPrefix"
Write-Host "Workspace   : $WorkspaceRoot"
Write-Host "CMake       : $CMake"
Write-Host "Ninja       : $Ninja"
Write-Host "MSVC env    : $VcVars"
Write-Host "Build type  : $BuildType"
Write-Host "Jobs        : $Jobs"
Write-Host "KF6 version : $Kf6Version"
Write-Host "ECM source  : $EcmSource"
Write-Host "ECM ref     : $EcmGitRef"

New-Item -ItemType Directory -Force -Path $WorkspaceRoot, $SdkPrefix, $sourceRoot, $buildRoot | Out-Null

if (!$SkipEcm) {
    Ensure-GitCheckout $EcmGitUrl $EcmGitRef $EcmSource

    if ($Clean) {
        Reset-Directory $ecmBuild $buildRoot
    } else {
        New-Item -ItemType Directory -Force -Path $ecmBuild | Out-Null
    }

    $prefixPath = "$QtPrefix;$SdkPrefix"
    $configure = @(
        """$CMake""",
        "-S", """$EcmSource""",
        "-B", """$ecmBuild""",
        "-G", "Ninja",
        "-DCMAKE_MAKE_PROGRAM=""$Ninja""",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_INSTALL_PREFIX=""$SdkPrefix""",
        "-DCMAKE_PREFIX_PATH=""$prefixPath""",
        "-DBUILD_TESTING=OFF"
    ) -join " "
    Invoke-VsCmd $configure

    $build = """$CMake"" --build ""$ecmBuild"" --target install --parallel $Jobs"
    Invoke-VsCmd $build
}

$envScript = Join-Path $SdkPrefix "scholia-sdk-env.ps1"
$envContent = @"
`$env:SCHOLIA_SDK_PREFIX = '$SdkPrefix'
`$env:QTDIR = '$QtPrefix'
`$env:PATH = '$QtPrefix\bin;$SdkPrefix\bin;' + `$env:PATH
`$env:CMAKE_PREFIX_PATH = '$QtPrefix;$SdkPrefix'
`$env:QT_PLUGIN_PATH = '$QtPrefix\plugins;$SdkPrefix\plugins'
Write-Host "Scholia SDK environment loaded:"
Write-Host "  QtPrefix : `$env:QTDIR"
Write-Host "  SdkPrefix: `$env:SCHOLIA_SDK_PREFIX"
"@
$envContent | Set-Content -LiteralPath $envScript -Encoding UTF8

$manifest = [ordered]@{
    Generated = (Get-Date -Format o)
    QtPrefix = $QtPrefix
    SdkPrefix = $SdkPrefix
    WorkspaceRoot = $WorkspaceRoot
    CMake = $CMake
    Ninja = $Ninja
    VcVars = $VcVars
    Kf6Version = $Kf6Version
    EcmSource = $EcmSource
    EcmGitRef = $EcmGitRef
}
$manifestPath = Join-Path $SdkPrefix "scholia-sdk-bootstrap.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host ""
Write-Host "SDK bootstrap complete." -ForegroundColor Green
Write-Host "Environment script: $envScript"
Write-Host "Manifest          : $manifestPath"
