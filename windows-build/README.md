# Scholia Windows Standalone Build

This directory contains the Windows standalone build and packaging scripts for
this Scholia checkout. Run the commands below from the repository root.

Build output, SDK files, install trees, and package staging output live outside
the source checkout under the sibling `..\windows_build` directory.

## Layout

- Source tree: `.`
- Build workspace: `..\windows_build`
- SDK prefix: `..\windows_build\sdk`
- Scholia build dir: `..\windows_build\build\scholia-standalone`
- Scholia install dir: `..\windows_build\install\scholia`
- Package stage: `..\windows_build\dist\scholia-pdf\app`
- Package output: `..\windows_build\dist`

## Requirements

- Windows 10/11 x64.
- Visual Studio 2022 Build Tools with MSVC and the Windows SDK.
- Qt for MSVC, for example `C:\Qt\6.11.1\msvc2022_64`.
- Git.
- Inno Setup 6, only when building the installer.
- Initialized third-party sources:

```powershell
git submodule update --init --recursive external/poppler
```

## Build The SDK

The standalone build uses a local SDK prefix for ECM, KF6 modules, zlib,
freetype, libintl shim, helper tools, and custom Poppler.

Bootstrap ECM:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\bootstrap-kf6-sdk.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

Build required support libraries and tools:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-zlib-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-freetype-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-libintl-shim-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\install-winflexbison-sdk.ps1
```

Build the required KF6 modules into `..\windows_build\sdk`. Use
`windows-build\KF6_SDK_BOOTSTRAP.md` for the module list and per-module notes.

Build custom Poppler:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-poppler-sdk.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

## Build Scholia

Incremental standalone build and install:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-scholia-standalone.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

Clean reconfigure and rebuild:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-scholia-standalone.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64 `
  -Clean
```

Run the installed local build:

```powershell
..\windows_build\install\scholia\bin\scholia.exe
```

Run with TeX invocation logging enabled:

```powershell
$env:QT_LOGGING_RULES = 'org.jairy.scholia.ui.debug=true'
..\windows_build\install\scholia\bin\scholia.exe
```

## Deploy Runtime Files

After a build, deploy Qt and SDK runtime files into the install tree:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\deploy-scholia-standalone-runtime.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

This keeps runtime plugins under `bin\plugins` and removes stale duplicate
plugin locations from the install prefix.

## Smoke Test A Staged Runtime

The package smoke test runs from `..\windows_build\dist\scholia-pdf\app`.
Prepare that stage through the installer script, or copy the install tree there
manually while debugging.

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\smoke-test-okular-pdf-only-stage.ps1 `
  -PdfPath .\autotests\data\file2.pdf
```

The smoke test verifies that the staged `scholia.exe` starts, opens the PDF, and
loads project-local modules from the stage rather than from another workspace
tree.

## Build Installer

Full build, deploy, stage, and installer:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

If the installed runtime has already been built and deployed:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 `
  -SkipBuild `
  -SkipDeploy
```

If the stage already passed the smoke test:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 `
  -SkipStage
```

Expected installer output:

```text
..\windows_build\dist\Scholia-<version>-Setup.exe
```

## LaTeX Notes

Scholia supports the StemTeX renderer for LaTeX notes. The standalone Windows
runtime bundles StemTeX under `StemTeX\runtime` with profiles under
`StemTeX\profiles`; Scholia starts that bundled renderer during application
startup.

The StemTeX profile and TeXLive package/font tree can be selected from
`Settings -> Configure Scholia -> Annotations`. Leave the TeX tree empty to use
the bundled tree under `StemTeX\runtime`; choose another directory when testing a
different TeX distribution. Environment variables such as
`SCHOLIA_STEMTEX_PROFILE_NAME` and `SCHOLIA_STEMTEX_TEXMF_ROOT` still override
the saved UI settings for diagnostics.

To inspect TeX rendering logs:

```powershell
Get-Content "$env:LOCALAPPDATA\scholia\scholia-tex-debug.log" -Tail 80
```

Expected log operations include:

- `stemtex-render` for the StemTeX backend.
