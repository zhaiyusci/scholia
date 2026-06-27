# Scholia Windows Build

This directory contains the Windows standalone SDK, runtime deployment, staging,
and installer scripts for this Scholia checkout.

Run commands from the repository root:

```powershell
cd C:\Users\jairy\Documents\okular\scholia
```

Build output lives outside the source checkout under the sibling
`..\windows_build` directory.

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
- MSVC x64 toolchain and Windows SDK. Visual Studio 2022 or newer is fine when
  passed through `-VcVars`.
- Qt for MSVC, for example `C:\Qt\6.11.1\msvc2022_64`.
- Git.
- Inno Setup 6, only when building the installer `.exe`.
- Initialized third-party sources:

```powershell
git submodule update --init --recursive external/poppler
```

If using a non-default Visual Studio path, pass it to build scripts:

```powershell
-VcVars "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
```

## Canonical Build

Use this script for normal Windows work:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-scholia-installer.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64 `
  -VcVars "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" `
  -WorkspaceRoot C:\Users\jairy\Documents\okular\windows_build `
  -SdkPrefix C:\Users\jairy\Documents\okular\windows_build\sdk
```

This is the single user-facing Windows entry point. It runs:

1. `build-scholia-standalone.ps1`
2. `deploy-scholia-standalone-runtime.ps1`
3. stage refresh under `..\windows_build\dist\scholia-pdf\app`
4. Inno Setup installer build

If Inno Setup is not installed, still build and stage the runtime with:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-scholia-installer.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64 `
  -VcVars "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" `
  -WorkspaceRoot C:\Users\jairy\Documents\okular\windows_build `
  -SdkPrefix C:\Users\jairy\Documents\okular\windows_build\sdk `
  -SkipInstaller
```

Expected installer output:

```text
..\windows_build\dist\Scholia-<version>-Setup.exe
```

## Internal Scripts

These scripts are internal steps. Use them only when debugging a specific part
of the Windows pipeline.

- `build-scholia-standalone.ps1`
  - Configures, builds, and installs Scholia into
    `..\windows_build\install\scholia`.
  - Produces CMake-installed files, including application translations and
    Scholia annotation resources.
- `deploy-scholia-standalone-runtime.ps1`
  - Copies Qt, KF6, Poppler, QScintilla, and StemTeX runtime files into the
    install tree.
  - Normalizes plugin layout under `bin\plugins`.
  - Restores Scholia runtime data after SDK data sync.
- `smoke-test-scholia-stage.ps1`
  - Starts staged `scholia.exe` with a test PDF and verifies it loads modules
    from the stage, not from another workspace tree.

The legacy Okular-named entry points were removed from this Windows line. New
docs should point at the Scholia-named scripts above.

## Stage-Only Refresh

If the install tree has already been built and deployed, refresh the stage
without rebuilding the installer:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-scholia-installer.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64 `
  -WorkspaceRoot C:\Users\jairy\Documents\okular\windows_build `
  -SdkPrefix C:\Users\jairy\Documents\okular\windows_build\sdk `
  -SkipBuild `
  -SkipDeploy `
  -SkipInstaller
```

If a staged StemTeX process is still running, stop it before staging:

```powershell
Get-Process | Where-Object { $_.ProcessName -match 'scholia|stemtex|xetex|xelatex|xetexdaemon' } |
  Stop-Process -Force
```

## Runtime Data Contract

`deploy-scholia-standalone-runtime.ps1` intentionally mirrors SDK runtime data
from `..\windows_build\sdk\bin\data` into `bin\data`. That SDK sync can remove
files installed earlier by CMake, so the deploy script must restore Scholia data
after the sync.

The deployed install tree and stage must contain:

- `bin\scholia.exe`
- `bin\scholia.ico`
- `bin\data\applications\org.jairy.scholia.desktop`
- `bin\data\metainfo\org.jairy.scholia.appdata.xml`
- `bin\data\icons\hicolor\<size>x<size>\apps\scholia.png`
- `bin\data\scholia\tools.xml`
- `bin\data\scholia\toolsQuick.xml`
- `bin\data\scholia\drawingtools.xml`
- `bin\data\scholia\pics\annotation-*.svg`
- `bin\data\locale\<lang>\LC_MESSAGES\okular*.mo`
- `share\poppler\cMap\...`
- `share\poppler\cidToUnicode\...`
- `StemTeX\runtime`
- `StemTeX\gui\profiles`

Do not rely on `build-scholia-standalone.ps1` alone for a runnable/packageable
Windows tree. Always run deploy, or use the canonical
`build-scholia-installer.ps1` entry point.

## Validation

Run the staged smoke test:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\smoke-test-scholia-stage.ps1 `
  -PdfPath .\autotests\data\file2.pdf
```

Quick file checks:

```powershell
$stage = "C:\Users\jairy\Documents\okular\windows_build\dist\scholia-pdf\app"
Test-Path "$stage\bin\data\scholia\pics\annotation-latex-note.svg"
Test-Path "$stage\bin\data\locale\zh_CN\LC_MESSAGES\okular.mo"
Test-Path "$stage\share\poppler\cMap\Adobe-GB1\UniGB-UTF16-H"
Test-Path "$stage\bin\data\icons\hicolor\256x256\apps\scholia.png"
```

## SDK Bootstrap

The standalone build uses a local SDK prefix for ECM, KF6 modules, zlib,
freetype, libintl shim, helper tools, and custom Poppler.

Bootstrap ECM:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\bootstrap-kf6-sdk.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

Build support libraries and tools:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-zlib-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-freetype-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\install-gettext-native-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-libintl-shim-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\install-winflexbison-sdk.ps1
```

Build required KF6 modules into `..\windows_build\sdk`. Use
`windows-build\KF6_SDK_BOOTSTRAP.md` for the module list and per-module notes.

Build custom Poppler:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-poppler-sdk.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

`install-gettext-native-sdk.ps1` installs pinned native Windows
`gettext-iconv-windows` tools under
`..\windows_build\sdk\tools\gettext-native`. Scholia uses that `msgfmt.exe` to
compile real gettext catalogs; the libintl shim is only the runtime
compatibility layer needed by KI18n.

## LaTeX Notes

Scholia supports the StemTeX renderer for LaTeX notes. The standalone Windows
runtime bundles StemTeX under `StemTeX\runtime` with profiles under
`StemTeX\gui\profiles`; Scholia starts that bundled renderer during application
startup.

The StemTeX profile and TeXLive package/font tree can be selected from
`Settings -> Configure Scholia -> Annotations`. Leave the TeX tree empty to use
the bundled tree under `StemTeX\runtime`.

To inspect TeX rendering logs:

```powershell
Get-Content "$env:LOCALAPPDATA\scholia\scholia-tex-debug.log" -Tail 80
```
