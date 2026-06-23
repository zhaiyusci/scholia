[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $InstallPrefix = "",
    [string] $StageRoot = "",
    [string] $OutputDir = "",
    [string] $Version = "",
    [string] $FileVersion = "",
    [int] $Jobs = 8,
    [string] $ISCC = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    [switch] $SkipBuild,
    [switch] $SkipDeploy,
    [switch] $SkipStage
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (!$WorkspaceRoot) {
    $WorkspaceRoot = Join-Path (Split-Path -Parent $repoRoot) "windows_build"
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)
if (!$InstallPrefix) {
    $InstallPrefix = Join-Path $WorkspaceRoot "install\scholia"
}
$InstallPrefix = [System.IO.Path]::GetFullPath($InstallPrefix)
if (!$SdkPrefix) {
    $SdkPrefix = Join-Path $WorkspaceRoot "sdk"
}
$SdkPrefix = [System.IO.Path]::GetFullPath($SdkPrefix)
if (!$StageRoot) {
    $StageRoot = Join-Path $WorkspaceRoot "dist\scholia-pdf\app"
}
if (!$OutputDir) {
    $OutputDir = Join-Path $WorkspaceRoot "dist"
}
$StageRoot = [System.IO.Path]::GetFullPath($StageRoot)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

function Remove-DirectoryInside([string] $Path, [string] $AllowedRoot) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $allowed = [System.IO.Path]::GetFullPath($AllowedRoot)
    if (!$full.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove directory outside $allowed`: $full"
    }
    if (Test-Path -LiteralPath $full) {
        Remove-Item -LiteralPath $full -Recurse -Force
    }
}

function Copy-DirectoryContents([string] $Source, [string] $Destination) {
    if (!(Test-Path -LiteralPath $Source)) {
        throw "Cannot find source directory: $Source"
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}

function Assert-StemTeXBundleLayout([string] $Root) {
    $requiredPaths = @(
        "runtime\bin\sdk\stemtex-renderer.dll",
        "runtime\bin\windows\xetexdaemon.exe",
        "gui\profiles"
    )
    foreach ($relativePath in $requiredPaths) {
        $path = Join-Path $Root $relativePath
        if (!(Test-Path -LiteralPath $path)) {
            throw "StemTeX bundle has an unsupported layout. Missing $relativePath under $Root. Run deploy-scholia-standalone-runtime.ps1 first."
        }
    }
}

function Copy-RuntimeStage([string] $SourcePrefix, [string] $DestinationRoot) {
    $sourceBin = Join-Path $SourcePrefix "bin"
    if (!(Test-Path -LiteralPath (Join-Path $sourceBin "scholia.exe"))) {
        throw "Cannot find deployed Scholia runtime under $sourceBin. Run deploy-scholia-standalone-runtime.ps1 first."
    }

    New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null
    Copy-DirectoryContents $sourceBin (Join-Path $DestinationRoot "bin")

    $sourceStemTeX = Join-Path $SourcePrefix "StemTeX"
    if (Test-Path -LiteralPath $sourceStemTeX) {
        Assert-StemTeXBundleLayout $sourceStemTeX
        Copy-DirectoryContents $sourceStemTeX (Join-Path $DestinationRoot "StemTeX")
    } else {
        Write-Warning "StemTeX runtime was not found under $sourceStemTeX. LaTeX StemTeX backend will be unavailable in the installer stage."
    }
}

function Invoke-ChildScript([string] $ScriptPath, [string[]] $Arguments) {
    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & powershell.exe -ExecutionPolicy Bypass -NoProfile -File $ScriptPath @Arguments
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($exitCode -ne 0) {
        exit $exitCode
    }
}

function Read-ScholiaVersion([string] $Root) {
    $versionFile = Join-Path $Root "VERSION.txt"
    if (!(Test-Path -LiteralPath $versionFile)) {
        throw "Cannot find Scholia version file: $versionFile"
    }
    $value = (Get-Content -LiteralPath $versionFile -Raw).Trim()
    if ($value -notmatch '^\d+\.\d+\.\d+$') {
        throw "VERSION.txt must contain MAJOR.MINOR.PATCH, got '$value'"
    }
    return $value
}

$repoVersion = Read-ScholiaVersion $repoRoot
if (!$Version) {
    $Version = $repoVersion
}
if (!$FileVersion) {
    $FileVersion = "$repoVersion.0"
}

if (!$SkipStage) {
    if (!$SkipBuild) {
        $buildArgs = @(
            "-WorkspaceRoot", $WorkspaceRoot,
            "-InstallPrefix", $InstallPrefix,
            "-Jobs", $Jobs
        )
        if ($QtPrefix) {
            $buildArgs += @("-QtPrefix", $QtPrefix)
        }
        $buildArgs += @("-SdkPrefix", $SdkPrefix)
        Invoke-ChildScript (Join-Path $PSScriptRoot "build-scholia-standalone.ps1") $buildArgs
    }

    if (!$SkipDeploy) {
        $deployArgs = @(
            "-WorkspaceRoot", $WorkspaceRoot,
            "-InstallPrefix", $InstallPrefix
        )
        if ($QtPrefix) {
            $deployArgs += @("-QtPrefix", $QtPrefix)
        }
        $deployArgs += @("-SdkPrefix", $SdkPrefix)
        Invoke-ChildScript (Join-Path $PSScriptRoot "deploy-scholia-standalone-runtime.ps1") $deployArgs
    }

    Remove-DirectoryInside $StageRoot $WorkspaceRoot
    Copy-RuntimeStage $InstallPrefix $StageRoot
}

if (!(Test-Path -LiteralPath $ISCC)) {
    throw "Cannot find Inno Setup compiler: $ISCC"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$env:SCHOLIA_STAGE = $StageRoot
$env:SCHOLIA_OUTPUT = $OutputDir
$env:SCHOLIA_VERSION = $Version
$env:SCHOLIA_FILE_VERSION = $FileVersion

$iss = Join-Path (Split-Path -Parent $PSScriptRoot) "installer\okular-pdf-only.iss"
Write-Host "Building installer with Inno Setup:"
Write-Host "  Script: $iss"
Write-Host "  Stage: $StageRoot"
Write-Host "  Output: $OutputDir"
Write-Host "  Version: $Version"
Write-Host "  File version: $FileVersion"
Write-Host ""

$oldErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $ISCC $iss
    $exitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $oldErrorActionPreference
}
exit $exitCode
