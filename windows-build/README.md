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

If using a non-default Visual Studio path, pass it to CMake scripts:

```powershell
-DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat"
```

## Canonical CMake Build

Use the CMake driver for normal Windows builds:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/build-windows.cmake
```

It runs:

1. CMake configure/build/install for Scholia
2. CMake runtime deployment from the SDK, Qt, StemTeX, and Scholia data
3. stage refresh under `..\windows_build\dist\scholia-pdf\app`
4. Inno Setup installer build

For a clean Scholia rebuild, add `-DCLEAN_BUILD=ON`.

If Inno Setup is not installed, still build and stage the runtime with:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -DSKIP_INSTALLER=ON `
  -P windows-build/cmake/build-windows.cmake
```

Expected installer output:

```text
..\windows_build\dist\Scholia-<version>-Setup.exe
```

## CMake Package Step

After `..\windows_build\install\scholia` has been built and deployed, the
package-only CMake script can recreate the stage and installer without
rebuilding Scholia:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DCLEAN_STAGE=ON `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/package-windows.cmake
```

This script mirrors the deployed install tree into
`..\windows_build\dist\scholia-pdf\app`, validates Scholia icons, gettext
catalogs, annotation resources, and Poppler CMap/CID data, then calls Inno
Setup. The main installer keeps the StemTeX renderer binaries and bundled
profiles, but excludes the TeX package/font tree. That tree is staged separately
under `..\windows_build\dist\scholia-stemtex-support\app` and built as
`Scholia-<version>-StemTeX-Support.exe`.

To validate staging without building an installer:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DSKIP_INSTALLER=ON `
  -DCLEAN_STAGE=ON `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/package-windows.cmake
```

To write the installer into a temporary output directory:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DCLEAN_STAGE=ON `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -DOUTPUT_DIR=C:/Users/jairy/Documents/okular/windows_build/tmp/cmake-package-test `
  -P windows-build/cmake/package-windows.cmake
```

## Internal Scripts

These scripts are internal steps. Use them only when debugging a specific part
of the Windows pipeline.

- `cmake\build-windows.cmake`
  - Canonical Windows driver for Scholia configure/build/install, runtime
    deploy, stage refresh, and installer creation.
- `cmake\deploy-runtime.cmake`
  - CMake-based runtime deployment from an installed Scholia tree.
- `cmake\package-windows.cmake`
  - CMake-based stage and installer creation from an already deployed
    `..\windows_build\install\scholia` tree.
  - Builds both the main Scholia installer and the optional StemTeX support
    installer when the deployed StemTeX TeX tree is available.
- `cmake\bootstrap-kf6-sdk.cmake`
  - CMake-based ECM bootstrap for the local Windows SDK.
- `cmake\build-kf6-module.cmake`
  - CMake-based KF6 module checkout, configure, build, and install driver.
- `cmake\build-zlib-sdk.cmake`, `cmake\build-freetype-sdk.cmake`,
  `cmake\build-libintl-shim-sdk.cmake`, `cmake\build-poppler-sdk.cmake`
  - CMake-based support library builders.
- `cmake\install-gettext-native-sdk.cmake`,
  `cmake\install-winflexbison-sdk.cmake`
  - CMake-based pinned binary tool installers.
- `build-scholia-standalone.ps1`
  - Legacy PowerShell build entry. Configures, builds, and installs Scholia into
    `..\windows_build\install\scholia`.
  - Produces CMake-installed files, including application translations and
    Scholia annotation resources.
- `deploy-scholia-standalone-runtime.ps1`
  - Legacy PowerShell deploy entry. Copies Qt, KF6, Poppler, QScintilla, and
    StemTeX runtime files into the install tree.
  - Normalizes plugin layout under `bin\plugins`.
  - Restores Scholia runtime data after SDK data sync.
- `smoke-test-scholia-stage.ps1`
  - Starts staged `scholia.exe` with a test PDF and verifies it loads modules
    from the stage, not from another workspace tree.

The legacy Okular-named entry points were removed from this Windows line. New
docs should point at the Scholia-named scripts above.

## Stage-Only Refresh

If the install tree has already been built and deployed, use the CMake package
script to refresh the stage without rebuilding the installer:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DSKIP_INSTALLER=ON `
  -DCLEAN_STAGE=ON `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/package-windows.cmake
```

If a staged StemTeX process is still running, stop it before staging:

```powershell
Get-Process | Where-Object { $_.ProcessName -match 'scholia|stemtex|xetex|xelatex|xetexdaemon' } |
  Stop-Process -Force
```

## Runtime Data Contract

The CMake runtime deployment intentionally mirrors SDK runtime data from
`..\windows_build\sdk\bin\data` into `bin\data`. That SDK sync can remove files
installed earlier by CMake, so deployment must restore Scholia data after the
sync.

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
- `StemTeX\runtime\bin`
- `StemTeX\gui\profiles`

The main stage must not contain `StemTeX\runtime\texmf-dist` or writable
StemTeX cache/state directories such as `StemTeX\runtime\texmf-var\fonts`.
The xelatex daemon format under `StemTeX\runtime\texmf-var\web2c` is a read-only
runtime input and remains in the main package.

Do not rely on the CMake install step alone for a runnable/packageable Windows
tree. Always run runtime deployment, or use the canonical
`windows-build\cmake\build-windows.cmake` entry point. For packaging an already
deployed tree, use `windows-build\cmake\package-windows.cmake`.

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
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/bootstrap-kf6-sdk.cmake
```

Build support libraries and tools:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/install-gettext-native-sdk.cmake

& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/install-winflexbison-sdk.cmake

& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/build-zlib-sdk.cmake

& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/build-freetype-sdk.cmake

& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/build-libintl-shim-sdk.cmake
```

Build required KF6 modules into `..\windows_build\sdk` with the CMake module
driver:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DMODULE=kcoreaddons `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/build-kf6-module.cmake
```

Use `windows-build\KF6_SDK_BOOTSTRAP.md` for the module list and per-module
notes.

Build custom Poppler:

```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" `
  -DQT_PREFIX=C:/Qt/6.11.1/msvc2022_64 `
  -DVCVARS="C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" `
  -DWORKSPACE_ROOT=C:/Users/jairy/Documents/okular/windows_build `
  -P windows-build/cmake/build-poppler-sdk.cmake
```

`install-gettext-native-sdk.cmake` installs pinned native Windows
`gettext-iconv-windows` tools under
`..\windows_build\sdk\tools\gettext-native`. Scholia uses that `msgfmt.exe` to
compile real gettext catalogs; the libintl shim is only the runtime
compatibility layer needed by KI18n.

## LaTeX Notes

Scholia supports the StemTeX renderer for LaTeX notes. The standalone Windows
runtime bundles the StemTeX renderer binaries under `StemTeX\runtime\bin` with
profiles under `StemTeX\gui\profiles`; Scholia starts that renderer during
application startup.

The TeXLive package/font tree is optional. Users can install
`Scholia-<version>-StemTeX-Support.exe` to add the bundled `texmf-dist` and
`texmf-var` tree under `StemTeX\runtime`, or they can select their own TeX tree
from `Settings -> Configure Scholia -> Annotations`. An empty setting does not
probe the system for TeX; it only uses the bundled support tree when that tree
has been installed.

StemTeX runtime state, fontconfig files, caches, traces, and rendered-note
outputs are written below Scholia's per-user temporary StemTeX directory, not
below the installation directory. This keeps `C:\Program Files\Scholia`
read-only after installation.

To inspect TeX rendering logs:

```powershell
Get-Content "$env:LOCALAPPDATA\scholia\scholia-tex-debug.log" -Tail 80
```
