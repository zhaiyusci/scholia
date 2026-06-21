[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $InstallPrefix = "",
    [string] $StemTeXRoot = "",
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

function Remove-FileInside([string] $Path, [string] $AllowedRoot) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $allowed = [System.IO.Path]::GetFullPath($AllowedRoot)
    if (!$full.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove file outside $allowed`: $full"
    }
    if (Test-Path -LiteralPath $full) {
        Remove-Item -LiteralPath $full -Force
    }
}

function Remove-LegacyLatexRuntimeArtifacts([string] $Prefix) {
    Write-Host "Removing legacy LaTeX runtime artifacts..." -ForegroundColor Cyan
    Remove-DirectoryInside (Join-Path $Prefix "bin\data\scholia\microtex") $Prefix
    Remove-FileInside (Join-Path $Prefix "bin\LaTeX.dll") $Prefix
    Remove-FileInside (Join-Path $Prefix "lib\LaTeX.lib") $Prefix
}

function Remove-ExpandedBreezeIconTheme([string] $Prefix) {
    Write-Host "Removing expanded Breeze icon theme data..." -ForegroundColor Cyan
    Remove-DirectoryInside (Join-Path $Prefix "bin\data\icons\breeze") $Prefix
    Remove-DirectoryInside (Join-Path $Prefix "bin\data\icons\breeze-dark") $Prefix
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

function Find-StemTeXRoot([string] $RequestedRoot) {
    $candidates = @()
    if ($RequestedRoot) {
        $candidates += $RequestedRoot
    }
    if ($env:SCHOLIA_STEMTEX_SOURCE_ROOT) {
        $candidates += $env:SCHOLIA_STEMTEX_SOURCE_ROOT
    }
    $documentsRoot = Split-Path -Parent (Split-Path -Parent $repoRoot)
    $candidates += Join-Path $documentsRoot "xetex\stemtex"

    foreach ($candidate in ($candidates | Where-Object { $_ } | Select-Object -Unique)) {
        $full = [System.IO.Path]::GetFullPath($candidate)
        if ((Test-Path -LiteralPath (Join-Path $full "cpp-daemon\stemtex_renderer.h")) -and
            ((Test-Path -LiteralPath (Join-Path $full "staging\runtime")) -or (Test-Path -LiteralPath (Join-Path $full "dist\stemtex-installer\StemTeX\runtime")))) {
            return $full
        }
    }

    throw "Cannot find StemTeX source/staging tree. Pass -StemTeXRoot or set SCHOLIA_STEMTEX_SOURCE_ROOT."
}

function Resolve-StemTeXRuntimeSource([string] $Root) {
    foreach ($candidate in @(
        (Join-Path $Root "staging\runtime"),
        (Join-Path $Root "dist\stemtex-installer\StemTeX\runtime"),
        (Join-Path $Root "dist\stemtex-texlive-daemon-static")
    )) {
        if ((Test-Path -LiteralPath (Join-Path $candidate "bin\sdk\stemtex-renderer.dll")) -and
            (Test-Path -LiteralPath (Join-Path $candidate "bin\windows\xetexdaemon.exe")) -and
            (Test-Path -LiteralPath (Join-Path $candidate "worker-template.tex"))) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw "Cannot find a staged StemTeX runtime under $Root."
}

function Resolve-StemTeXProfilesSource([string] $Root) {
    foreach ($candidate in @(
        (Join-Path $Root "staging\gui\profiles"),
        (Join-Path $Root "dist\stemtex-installer\StemTeX\gui\profiles"),
        (Join-Path $Root "gui\profiles")
    )) {
        if ((Test-Path -LiteralPath $candidate) -and (Get-ChildItem -LiteralPath $candidate -Directory -ErrorAction SilentlyContinue | Where-Object {
                    (Test-Path -LiteralPath (Join-Path $_.FullName "preamble.tex")) -and
                    (Test-Path -LiteralPath (Join-Path $_.FullName "warmup.tex"))
                } | Select-Object -First 1)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw "Cannot find staged StemTeX profiles under $Root."
}

$QtPrefix = Find-QtPrefix $QtPrefix
$WinDeployQt = Resolve-CommandPath "windeployqt" @($WinDeployQt, (Join-Path $QtPrefix "bin\windeployqt.exe"))
$StemTeXRoot = Find-StemTeXRoot $StemTeXRoot
$StemTeXRuntimeSource = Resolve-StemTeXRuntimeSource $StemTeXRoot
$StemTeXProfilesSource = Resolve-StemTeXProfilesSource $StemTeXRoot

$binDir = Join-Path $InstallPrefix "bin"
$exe = Join-Path $binDir "scholia.exe"
if (!(Test-Path -LiteralPath $exe)) {
    throw "Cannot find installed Scholia executable: $exe. Run build-scholia-standalone.ps1 first."
}

Write-Host "Scholia standalone runtime deploy" -ForegroundColor Green
Write-Host "QtPrefix  : $QtPrefix"
Write-Host "SdkPrefix : $SdkPrefix"
Write-Host "Install   : $InstallPrefix"
Write-Host "StemTeX   : $StemTeXRoot"

Remove-LegacyLatexRuntimeArtifacts $InstallPrefix

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
$oldErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    $winDeployQtOutput = & $WinDeployQt --no-translations --no-system-d3d-compiler --no-opengl-sw $exe 2>&1
    $winDeployQtExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $oldErrorActionPreference
}
$winDeployQtOutput | ForEach-Object { Write-Host $_ }
if ($winDeployQtExitCode -ne 0) {
    throw "windeployqt failed with exit code $winDeployQtExitCode"
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

Write-Host "Copying bundled StemTeX runtime..."
$stemTeXDestination = Join-Path $InstallPrefix "StemTeX"
Remove-DirectoryInside $stemTeXDestination $InstallPrefix
Copy-DirectoryContents $StemTeXRuntimeSource (Join-Path $stemTeXDestination "runtime")
Copy-DirectoryContents $StemTeXProfilesSource (Join-Path $stemTeXDestination "profiles")

Remove-ExpandedBreezeIconTheme $InstallPrefix
Remove-DirectoryInside (Join-Path $InstallPrefix "plugins") $InstallPrefix
Remove-DirectoryInside (Join-Path $InstallPrefix "lib\plugins") $InstallPrefix

Write-Host ""
Write-Host "Runtime deployment complete." -ForegroundColor Green
