[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Module,
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $BuildType = "RelWithDebInfo",
    [int] $Jobs = 8,
    [string] $Kf6Version = "6.27.0",
    [string] $GitRef = "",
    [string] $GitUrl = "",
    [string] $CMake = "",
    [string] $Ninja = "",
    [string] $VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    [string[]] $ExtraCMakeArgs = @(),
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

function Resolve-NativeMsgFmt([string] $SdkPrefix) {
    $msgfmt = [System.IO.Path]::GetFullPath((Join-Path $SdkPrefix "tools\gettext-native\bin\msgfmt.exe"))
    if (!(Test-Path -LiteralPath $msgfmt)) {
        throw "Cannot find native Windows msgfmt at $msgfmt. Run windows-build\scripts\install-gettext-native-sdk.ps1 first."
    }

    $versionOutput = & $msgfmt --version 2>&1
    if ($LASTEXITCODE -ne 0 -or (($versionOutput -join "`n") -notmatch "GNU gettext-tools")) {
        throw "Configured msgfmt is not GNU gettext-tools: $msgfmt"
    }

    return $msgfmt
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
        $candidates += Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qt6Dir))
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
    Write-Host "Cloning ${Module}: $Url -> $Destination"
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
$MsgFmt = Resolve-NativeMsgFmt $SdkPrefix
if (!(Test-Path -LiteralPath $VcVars)) {
    throw "Cannot find vcvars64.bat: $VcVars"
}
if (!(Test-Path -LiteralPath (Join-Path $SdkPrefix "share\ECM\cmake\ECMConfig.cmake"))) {
    throw "ECM is not installed in $SdkPrefix. Run bootstrap-kf6-sdk.ps1 first."
}

if (!$GitRef) {
    $GitRef = "v$Kf6Version"
}
if (!$GitUrl) {
    $GitUrl = "https://invent.kde.org/frameworks/$Module.git"
}

$sourceRoot = Join-Path $WorkspaceRoot "sources"
$buildRoot = Join-Path $WorkspaceRoot "build\kf6-sdk"
$moduleSource = [System.IO.Path]::GetFullPath((Join-Path $sourceRoot $Module))
$moduleBuild = [System.IO.Path]::GetFullPath((Join-Path $buildRoot $Module))

Write-Host "Scholia KF6 module build" -ForegroundColor Green
Write-Host "Module      : $Module"
Write-Host "Git URL     : $GitUrl"
Write-Host "Git ref     : $GitRef"
Write-Host "QtPrefix    : $QtPrefix"
Write-Host "SdkPrefix   : $SdkPrefix"
Write-Host "Workspace   : $WorkspaceRoot"
Write-Host "CMake       : $CMake"
Write-Host "Ninja       : $Ninja"
Write-Host "msgfmt      : $MsgFmt"
Write-Host "Build type  : $BuildType"
Write-Host "Jobs        : $Jobs"
if ($ExtraCMakeArgs.Count -gt 0) {
    $ExtraCMakeArgs = @($ExtraCMakeArgs | ForEach-Object { $_ -split "," } | Where-Object { $_ })
    Write-Host "Extra CMake : $($ExtraCMakeArgs -join ' ')"
}

Ensure-GitCheckout $GitUrl $GitRef $moduleSource

if ($Clean) {
    Reset-Directory $moduleBuild $buildRoot
} else {
    New-Item -ItemType Directory -Force -Path $moduleBuild | Out-Null
}

$prefixPath = "$QtPrefix;$SdkPrefix"
$configure = @(
    """$CMake""",
    "-S", """$moduleSource""",
    "-B", """$moduleBuild""",
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=""$Ninja""",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_INSTALL_PREFIX=""$SdkPrefix""",
    "-DCMAKE_PREFIX_PATH=""$prefixPath""",
    "-DGETTEXT_MSGFMT_EXECUTABLE=""$MsgFmt""",
    "-DBUILD_TESTING=OFF",
    "-DBUILD_QCH=OFF"
) -join " "
if ($ExtraCMakeArgs.Count -gt 0) {
    $configure = "$configure $($ExtraCMakeArgs -join ' ')"
}
Invoke-VsCmd $configure

$build = """$CMake"" --build ""$moduleBuild"" --target install --parallel $Jobs"
Invoke-VsCmd $build

$moduleManifest = Join-Path $SdkPrefix "scholia-sdk-modules.json"
$modules = @()
if (Test-Path -LiteralPath $moduleManifest) {
    $loadedModules = Get-Content -LiteralPath $moduleManifest -Raw | ConvertFrom-Json
    foreach ($loadedModule in @($loadedModules)) {
        if ($loadedModule.Module) {
            $modules += $loadedModule
        } elseif ($loadedModule.value) {
            foreach ($nestedModule in @($loadedModule.value)) {
                if ($nestedModule.Module) {
                    $modules += $nestedModule
                }
            }
        }
    }
}
$modules = @($modules | Where-Object { $_.Module -ne $Module })
$modules += [pscustomobject]@{
    Module = $Module
    GitUrl = $GitUrl
    GitRef = $GitRef
    BuildType = $BuildType
    InstalledAt = (Get-Date -Format o)
}
$modules | Sort-Object Module | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $moduleManifest -Encoding UTF8

Write-Host ""
Write-Host "KF6 module installed: $Module" -ForegroundColor Green
Write-Host "Module manifest: $moduleManifest"
