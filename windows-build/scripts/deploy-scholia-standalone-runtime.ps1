[CmdletBinding()]
param(
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WorkspaceRoot = "",
    [string] $InstallPrefix = "",
    [string] $StemTeXRoot = "",
    [string] $QScintillaRoot = "",
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

function Sync-DirectoryContents([string] $Source, [string] $Destination, [string] $AllowedRoot) {
    if (!(Test-Path -LiteralPath $Source)) {
        return
    }

    $destinationFull = [System.IO.Path]::GetFullPath($Destination)
    $allowed = [System.IO.Path]::GetFullPath($AllowedRoot)
    if (!$destinationFull.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to sync directory outside $allowed`: $destinationFull"
    }

    New-Item -ItemType Directory -Force -Path $destinationFull | Out-Null
    $robocopyArgs = @(
        [System.IO.Path]::GetFullPath($Source),
        $destinationFull,
        "/MIR",
        "/R:1",
        "/W:1",
        "/COPY:DAT",
        "/DCOPY:DAT",
        "/NFL",
        "/NDL",
        "/NJH",
        "/NJS",
        "/NP"
    )
    & robocopy @robocopyArgs | Out-Null
    $exitCode = $LASTEXITCODE
    if ($exitCode -ge 8) {
        throw "robocopy failed with exit code $exitCode while syncing $Source to $Destination"
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

function Remove-EmptyGettextCatalogs([string] $Root) {
    if (!(Test-Path -LiteralPath $Root)) {
        return
    }

    $emptyCatalogs = @(Get-ChildItem -LiteralPath $Root -Recurse -Filter "*.mo" -File -ErrorAction SilentlyContinue | Where-Object { $_.Length -le 28 })
    if ($emptyCatalogs.Count -eq 0) {
        return
    }

    Write-Host "Removing $($emptyCatalogs.Count) empty gettext catalogs under $Root..." -ForegroundColor Cyan
    $emptyCatalogs | Remove-Item -Force
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

function Copy-FileToDirectory([string] $Source, [string] $DestinationDir) {
    if (!(Test-Path -LiteralPath $Source)) {
        throw "Missing Scholia runtime data file: $Source"
    }
    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    Copy-Item -LiteralPath $Source -Destination $DestinationDir -Force
}

function Copy-ScholiaRuntimeData([string] $Prefix) {
    Write-Host "Copying Scholia runtime data..." -ForegroundColor Cyan
    $iconsRoot = Join-Path $script:repoRoot "icons"
    $dataIconsRoot = Join-Path $Prefix "bin\data\icons\hicolor"

    foreach ($size in @("16", "22", "32", "48", "64", "128", "256")) {
        $source = Join-Path $iconsRoot "$size-apps-scholia.png"
        if (!(Test-Path -LiteralPath $source)) {
            throw "Missing Scholia icon: $source"
        }
        $destinationDir = Join-Path $dataIconsRoot "$($size)x$size\apps"
        New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
        Copy-Item -LiteralPath $source -Destination (Join-Path $destinationDir "scholia.png") -Force

        $pdfIconSource = Join-Path $iconsRoot "$size-mimetypes-application-pdf.png"
        if (!(Test-Path -LiteralPath $pdfIconSource)) {
            throw "Missing Scholia PDF icon: $pdfIconSource"
        }
        $pdfIconDestinationDir = Join-Path $dataIconsRoot "$($size)x$size\mimetypes"
        New-Item -ItemType Directory -Force -Path $pdfIconDestinationDir | Out-Null
        Copy-Item -LiteralPath $pdfIconSource -Destination (Join-Path $pdfIconDestinationDir "application-pdf.png") -Force
    }

    $icoSource = Join-Path $iconsRoot "scholia.ico"
    if (!(Test-Path -LiteralPath $icoSource)) {
        throw "Missing Scholia icon: $icoSource"
    }
    Copy-Item -LiteralPath $icoSource -Destination (Join-Path $Prefix "bin\scholia.ico") -Force

    $pdfIcoSource = Join-Path $iconsRoot "application-pdf.ico"
    if (!(Test-Path -LiteralPath $pdfIcoSource)) {
        throw "Missing Scholia PDF icon: $pdfIcoSource"
    }
    Copy-Item -LiteralPath $pdfIcoSource -Destination (Join-Path $Prefix "bin\application-pdf.ico") -Force

    Copy-FileToDirectory (Join-Path $script:repoRoot "shell\org.jairy.scholia.desktop") (Join-Path $Prefix "bin\data\applications")
    Copy-FileToDirectory (Join-Path $script:repoRoot "shell\org.jairy.scholia.appdata.xml") (Join-Path $Prefix "bin\data\metainfo")
    Copy-FileToDirectory (Join-Path $script:repoRoot "scholia.categories") (Join-Path $Prefix "bin\data\qlogging-categories6")

    $scholiaDataDir = Join-Path $Prefix "bin\data\scholia"
    foreach ($fileName in @("tools.xml", "toolsQuick.xml", "drawingtools.xml")) {
        Copy-FileToDirectory (Join-Path $script:repoRoot "part\data\$fileName") $scholiaDataDir
    }

    $scholiaPicsDir = Join-Path $scholiaDataDir "pics"
    Copy-FileToDirectory (Join-Path $script:repoRoot "core\stamps.svg") $scholiaPicsDir
    $partDataDir = Join-Path $script:repoRoot "part\data"
    foreach ($file in Get-ChildItem -LiteralPath $partDataDir -File) {
        if ($file.Extension -in @(".png", ".svg")) {
            Copy-FileToDirectory $file.FullName $scholiaPicsDir
        }
    }
}

function Copy-ScholiaTranslations([string] $WorkspaceRoot, [string] $Prefix) {
    Write-Host "Copying Scholia translations..." -ForegroundColor Cyan
    $sourceRoot = Join-Path $WorkspaceRoot "build\scholia-standalone\locale"
    if (!(Test-Path -LiteralPath $sourceRoot)) {
        throw "Missing Scholia translation build directory: $sourceRoot"
    }

    $destinationRoot = Join-Path $Prefix "bin\data\locale"
    $catalogs = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -Filter "*.mo" -File -ErrorAction SilentlyContinue | Where-Object { $_.Length -gt 28 })
    if ($catalogs.Count -eq 0) {
        throw "No non-empty Scholia translation catalogs were found under $sourceRoot"
    }

    $sourceRootWithSlash = $sourceRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    foreach ($catalog in $catalogs) {
        $catalogPath = [System.IO.Path]::GetFullPath($catalog.FullName)
        if (!$catalogPath.StartsWith($sourceRootWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Translation catalog is outside $sourceRoot`: $catalogPath"
        }
        $relativePath = $catalogPath.Substring($sourceRootWithSlash.Length)
        $destination = Join-Path $destinationRoot $relativePath
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath $catalog.FullName -Destination $destination -Force
    }
    Write-Host "Copied $($catalogs.Count) Scholia translation catalogs." -ForegroundColor Cyan
}

function Copy-QtChineseTranslations([string] $QtRoot, [string] $DestinationBinDir) {
    $sourceDir = Join-Path $QtRoot "translations"
    if (!(Test-Path -LiteralPath $sourceDir)) {
        Write-Host "Qt translations directory not found: $sourceDir" -ForegroundColor DarkYellow
        return
    }

    $destinationDir = Join-Path $DestinationBinDir "translations"
    New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
    foreach ($fileName in @("qt_zh_CN.qm", "qtbase_zh_CN.qm")) {
        $source = Join-Path $sourceDir $fileName
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination $destinationDir -Force
        } else {
            Write-Host "Skipping missing Qt translation: $fileName" -ForegroundColor DarkYellow
        }
    }
}

function Write-QtRuntimeConfig([string] $DestinationBinDir) {
    @"
[Paths]
Prefix=.
Plugins=plugins
Translations=translations
"@ | Set-Content -LiteralPath (Join-Path $DestinationBinDir "qt.conf") -Encoding ASCII
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

function Find-QScintillaRoot([string] $RequestedRoot, [string] $StemTeXRoot) {
    $candidates = @()
    if ($RequestedRoot) {
        $candidates += $RequestedRoot
    }
    if ($env:SCHOLIA_QSCINTILLA_ROOT) {
        $candidates += $env:SCHOLIA_QSCINTILLA_ROOT
    }
    if ($StemTeXRoot) {
        $candidates += Join-Path $StemTeXRoot "third_party"
    }

    foreach ($candidate in ($candidates | Where-Object { $_ } | Select-Object -Unique)) {
        $full = [System.IO.Path]::GetFullPath($candidate)
        if ((Test-Path -LiteralPath (Join-Path $full "qscintilla-src\src\Qsci\qsciscintilla.h")) -and
            (Test-Path -LiteralPath (Join-Path $full "qscintilla-build\release\qscintilla2_qt6.dll"))) {
            return $full
        }
    }

    throw "Cannot find the StemTeX QScintilla runtime. Pass -QScintillaRoot or set SCHOLIA_QSCINTILLA_ROOT."
}

function Resolve-StemTeXRuntimeSource([string] $Root) {
    foreach ($candidate in @(
        (Join-Path $Root "staging\runtime"),
        (Join-Path $Root "dist\stemtex-installer\StemTeX\runtime"),
        (Join-Path $Root "dist\stemtex-texlive-daemon-static")
    )) {
        if ((Test-Path -LiteralPath (Join-Path $candidate "bin\sdk\stemtex-renderer.dll")) -and
            (Test-Path -LiteralPath (Join-Path $candidate "bin\windows\xetexdaemon.exe"))) {
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
$QScintillaRoot = Find-QScintillaRoot $QScintillaRoot $StemTeXRoot
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
Write-Host "QScintilla: $QScintillaRoot"

Remove-LegacyLatexRuntimeArtifacts $InstallPrefix

Write-Host ""
Write-Host "Copying SDK DLLs..."
Get-ChildItem -LiteralPath (Join-Path $SdkPrefix "bin") -Filter "*.dll" -File |
    Copy-Item -Destination $binDir -Force

Write-Host "Copying QScintilla runtime..."
Copy-Item -LiteralPath (Join-Path $QScintillaRoot "qscintilla-build\release\qscintilla2_qt6.dll") -Destination $binDir -Force

Write-Host "Copying SDK runtime data..."
Sync-DirectoryContents (Join-Path $SdkPrefix "bin\data") (Join-Path $binDir "data") $InstallPrefix
Remove-EmptyGettextCatalogs (Join-Path $binDir "data\locale")

$sdkPopplerData = Join-Path $SdkPrefix "share\poppler"
if (Test-Path -LiteralPath $sdkPopplerData) {
    Write-Host "Copying Poppler CMap/CID data..."
    Sync-DirectoryContents $sdkPopplerData (Join-Path $InstallPrefix "share\poppler") $InstallPrefix
} else {
    Write-Warning "Poppler CMap/CID data was not found under $sdkPopplerData. CJK PDFs may render incorrectly."
}

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

Write-Host "Copying Qt Chinese translations..."
Copy-QtChineseTranslations $QtPrefix $binDir
Write-QtRuntimeConfig $binDir

Write-Host "Copying bundled StemTeX runtime..."
$stemTeXDestination = Join-Path $InstallPrefix "StemTeX"
Sync-DirectoryContents $StemTeXRuntimeSource (Join-Path $stemTeXDestination "runtime") $InstallPrefix
Sync-DirectoryContents $StemTeXProfilesSource (Join-Path $stemTeXDestination "gui\profiles") $InstallPrefix

Remove-ExpandedBreezeIconTheme $InstallPrefix
Copy-ScholiaRuntimeData $InstallPrefix
Copy-ScholiaTranslations $WorkspaceRoot $InstallPrefix
Remove-DirectoryInside (Join-Path $InstallPrefix "plugins") $InstallPrefix
Remove-DirectoryInside (Join-Path $InstallPrefix "lib\plugins") $InstallPrefix

Write-Host ""
Write-Host "Runtime deployment complete." -ForegroundColor Green
