[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $InstallPrefix = "",
    [string] $WinDeployQt = ""
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

function Copy-DirectoryContents([string] $Source, [string] $Destination) {
    if (!(Test-Path -LiteralPath $Source)) {
        return
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}

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

function Copy-PluginFile([string] $SourceRoot, [string] $RelativePath, [string] $DestinationRoot) {
    $source = Join-Path $SourceRoot $RelativePath
    if (!(Test-Path -LiteralPath $source)) {
        Write-Host "Skipping missing plugin: $RelativePath" -ForegroundColor DarkYellow
        return
    }
    $destination = Join-Path $DestinationRoot $RelativePath
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

$QtPrefix = Find-QtPrefix $QtPrefix
$WinDeployQt = Resolve-CommandPath "windeployqt" @($WinDeployQt, (Join-Path $QtPrefix "bin\windeployqt.exe"))

$binDir = Join-Path $InstallPrefix "bin"
$exe = Join-Path $binDir "scholia.exe"
if (!(Test-Path -LiteralPath $exe)) {
    throw "Cannot find installed Scholia executable: $exe. Run build-scholia-standalone.ps1 first."
}

Write-Host "Scholia standalone runtime deploy" -ForegroundColor Green
Write-Host "QtPrefix  : $QtPrefix"
Write-Host "SdkPrefix : $SdkPrefix"
Write-Host "Install   : $InstallPrefix"

Write-Host ""
Write-Host "Copying SDK DLLs..."
Get-ChildItem -LiteralPath (Join-Path $SdkPrefix "bin") -Filter "*.dll" -File |
    Copy-Item -Destination $binDir -Force

Write-Host "Copying SDK runtime data..."
Copy-DirectoryContents (Join-Path $SdkPrefix "bin\data") (Join-Path $binDir "data")

Write-Host "Preparing runtime plugins..."
$runtimePluginsDir = Join-Path $binDir "plugins"
$stagedPluginsDir = Join-Path $InstallPrefix ".runtime-plugins-staging"
Remove-DirectoryInside $stagedPluginsDir $InstallPrefix

$sdkPluginsDir = Join-Path $SdkPrefix "lib\plugins"
foreach ($plugin in @(
    "kf6\kio\kio_file.dll",
    "kiconthemes6\iconengines\KIconEnginePlugin.dll"
)) {
    Copy-PluginFile $sdkPluginsDir $plugin $stagedPluginsDir
}

$installedPluginsDir = Join-Path $InstallPrefix "plugins"
if (Test-Path -LiteralPath $installedPluginsDir) {
    Copy-DirectoryContents $installedPluginsDir $stagedPluginsDir
} else {
    foreach ($plugin in @(
        "kf6\parts\okularpart.dll",
        "okular_generators\okularGenerator_poppler.dll"
    )) {
        Copy-PluginFile $runtimePluginsDir $plugin $stagedPluginsDir
    }
}

Remove-DirectoryInside $runtimePluginsDir $InstallPrefix
Move-Item -LiteralPath $stagedPluginsDir -Destination $runtimePluginsDir
Remove-DirectoryInside $installedPluginsDir $InstallPrefix
Remove-DirectoryInside (Join-Path $InstallPrefix "lib\plugins") $InstallPrefix

Write-Host "Removing stale Qt runtime DLLs..."
Get-ChildItem -LiteralPath $binDir -Filter "Qt6*.dll" -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

Write-Host "Running windeployqt..."
& $WinDeployQt --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Write-Host "Copying indirect Qt runtime DLLs..."
foreach ($dll in @(
    "Qt6Qml.dll",
    "Qt6QmlMeta.dll",
    "Qt6QmlModels.dll",
    "Qt6Quick.dll",
    "Qt6OpenGL.dll"
)) {
    $source = Join-Path (Join-Path $QtPrefix "bin") $dll
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination $binDir -Force
    } else {
        throw "Required Qt runtime DLL is missing from QtPrefix: $source"
    }
}

Remove-DirectoryInside (Join-Path $InstallPrefix "plugins") $InstallPrefix
Remove-DirectoryInside (Join-Path $InstallPrefix "lib\plugins") $InstallPrefix

Write-Host ""
Write-Host "Runtime deployment complete." -ForegroundColor Green
