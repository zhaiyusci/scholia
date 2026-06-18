# Scholia Windows Build

This directory contains the Windows build and packaging scripts for this Scholia
checkout. Run the commands below from the repository root.

Windows build logs and package staging output live outside the source checkout,
under the sibling `..\windows_build` directory. Do not create ad-hoc CMake build
directories inside the repository root.

## Quick Command Map

Use these commands from the repository root, unless noted otherwise.

Full Windows local build, PDF-only with MicroTeX:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 `
  -WorkspaceRoot ..\windows_build `
  -PdfOnly `
  -MicroTeXSrc .\external\MicroTeX
```

Incremental rebuild after ordinary `okularpart` source edits:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okularpart-incremental.ps1 `
  -SourceFiles part\latexrenderer.cpp
```

Incremental rebuild after Poppler implementation edits:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-poppler-incremental.ps1
```

Prepare, smoke test, and package the PDF-only installer:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\prepare-okular-pdf-only-stage.ps1

powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\smoke-test-okular-pdf-only-stage.ps1 `
  -PdfPath .\autotests\data\file2.pdf

powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 -SkipStage
```

Expected installer output:

```text
..\windows_build\dist\Scholia-PDF-26.07.70-patch<date>-Setup.exe
```

Run the installed local build:

```powershell
C:\CraftRoot\bin\scholia.exe
```

Run with TeX invocation logging enabled:

```powershell
$env:QT_LOGGING_RULES = 'org.jairy.scholia.ui.debug=true'
C:\CraftRoot\bin\scholia.exe
```

## Layout

- Scholia source: repository root, `.`
- Poppler source: `external\poppler`
- MicroTeX source: `external\MicroTeX`
- Windows build workspace: `..\windows_build`
- Craft root: `C:\CraftRoot`
- Scholia build dir: `C:\CraftRoot\build\kde\applications\okular\work\build`
- Poppler build dir: `C:\CraftRoot\build\qt-libs\poppler\work\build`
- Installed runtime:
  - `C:\CraftRoot\bin\scholia.exe`
  - `C:\CraftRoot\bin\Okular6Core.dll`
  - `C:\CraftRoot\plugins\kf6\parts\okularpart.dll`
  - `C:\CraftRoot\plugins\okular_generators\okularGenerator_poppler.dll`
- Build logs: `..\windows_build\build-logs`
- Package stage: `..\windows_build\dist\okular-pdf-only\app`
- Package output: `..\windows_build\dist`

## Requirements

- Windows 10/11 x64.
- Visual Studio 2022 Build Tools with MSVC and the Windows SDK.
- Git.
- KDE Craft installed at `C:\CraftRoot` with the
  `windows-cl-msvc2022-x86_64` ABI.
- Inno Setup 6, only when building the installer.
- `dumpbin.exe` from Visual Studio, only when preparing the package stage.
- `external\poppler` and `external\MicroTeX` initialized.
- `tinyxml2` installed into Craft for MicroTeX builds.

Initialize submodules:

```powershell
git submodule update --init --recursive external/poppler external/MicroTeX
```

If `tinyxml2` is missing from Craft, install it from the sibling checkout:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\install-tinyxml2-local.ps1
```

## Full PDF-Only Build

This is the normal full Windows build for testing `C:\CraftRoot\bin\scholia.exe`.
It builds local Poppler first, then builds Scholia with only the PDF generator and
with MicroTeX enabled.

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 `
  -WorkspaceRoot ..\windows_build `
  -PdfOnly `
  -MicroTeXSrc .\external\MicroTeX
```

Use this after changing dependencies, Poppler, CMake options, or when the build
directory should be treated as untrusted.

This repository's Windows full build means PDF-only. Do not remove `-PdfOnly`
for the normal Windows runtime or installer build.

To rebuild Scholia from scratch while reusing the already installed local
Poppler:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 `
  -WorkspaceRoot ..\windows_build `
  -SkipLocalPoppler `
  -PdfOnly `
  -MicroTeXSrc .\external\MicroTeX
```

The full-build script also makes sure Scholia's Craft work directory contains a
`poppler-local` junction pointing at the local Poppler build directory. The PDF
generator includes private Poppler headers from `external\poppler`, and those
headers need generated files such as `config.h` from the Poppler build tree.
Keep that link in place when reusing a local Poppler build; otherwise the PDF
generator may fail to compile even though `poppler.dll` and `poppler-qt6.dll`
are already installed.

Verify the configured options:

```powershell
Select-String C:\CraftRoot\build\kde\applications\okular\work\build\CMakeCache.txt `
  -Pattern '^(OKULAR_PDF_ONLY|OKULAR_ENABLE_MICROTEX|MICROTEX_SRC):'
```

Expected values:

```text
OKULAR_PDF_ONLY:BOOL=ON
OKULAR_ENABLE_MICROTEX:BOOL=ON
MICROTEX_SRC:PATH=<repo>/external/MicroTeX
```

Verify the installed files:

```powershell
Get-Item C:\CraftRoot\bin\scholia.exe,
         C:\CraftRoot\bin\Okular6Core.dll,
         C:\CraftRoot\plugins\kf6\parts\okularpart.dll,
         C:\CraftRoot\plugins\okular_generators\okularGenerator_poppler.dll,
         C:\CraftRoot\bin\poppler-qt6.dll |
  Select-Object FullName,Length,LastWriteTime

Test-Path C:\CraftRoot\bin\data\scholia\microtex\res
```

Run the build:

```powershell
C:\CraftRoot\bin\scholia.exe
```

## Incremental Scholia Rebuild

Use this for ordinary source edits in Scholia when the Craft build directory is
valid. It rebuilds the requested source files, relinks `okularpart.dll`, and
installs it into Craft.

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okularpart-incremental.ps1 `
  -SourceFiles part\latexrenderer.cpp
```

Pass every changed `.cpp` that belongs to `okularpart`. If the change touches
CMake configuration, generated headers, resources, Poppler, or MicroTeX, use the
full build instead.

For LaTeX note renderer changes, the usual incremental command is:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okularpart-incremental.ps1 `
  -SourceFiles part\latexrenderer.cpp
```

## Incremental Poppler Rebuild

Use this for Poppler implementation-only edits such as changes in
`external\poppler\poppler\*.cc` or `external\poppler\qt6\src\*.cc` when the
existing Poppler build directory is valid. It asks Ninja for the exact runtime
targets, then copies the rebuilt DLLs into Craft:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-poppler-incremental.ps1
```

The script intentionally does not run `ninja install`. In this CMake build,
`install` depends on `all`, so using it for a one-file Poppler fix can rebuild
unrelated GLib, C++, tests, or demo targets. The incremental script builds only:

- `poppler.dll`
- `qt6\src\poppler-qt6.dll`

It then installs the runtime files to:

- `C:\CraftRoot\bin\poppler.dll`
- `C:\CraftRoot\bin\poppler-qt6.dll`

No Scholia rebuild is needed for implementation-only Poppler changes because the
installed Scholia runtime dynamically loads those DLLs. Rebuild Scholia only when
Poppler headers, exported ABI, generator code, CMake options, or resources
changed.

To rebuild Poppler incrementally and then package the Windows installer from the
main package script:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\scripts\build-local-packages.ps1 `
  -SkipLinux `
  -WindowsPopplerIncremental
```

To verify the same main-script path without touching the installed runtime:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\scripts\build-local-packages.ps1 `
  -SkipLinux `
  -SkipWindowsPackage `
  -WindowsPopplerIncremental `
  -WindowsNoInstall
```

To package only from the already installed Windows runtime:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\scripts\build-local-packages.ps1 `
  -SkipLinux `
  -SkipWindowsBuild
```

## Clean Scholia Build Directory

Use this only when a full Scholia rebuild is needed. It removes Scholia's Craft
build and image directories, not the whole Craft root.

```powershell
$paths = @(
  'C:\CraftRoot\build\kde\applications\okular\work\build',
  'C:\CraftRoot\build\kde\applications\okular\image-RelWithDebInfo-26.04.1',
  'C:\CraftRoot\build\kde\applications\okular\image-RelWithDebInfo-26.04.1-dbg',
  'C:\CraftRoot\build\kde\applications\okular\image-RelWithDebInfo-local'
)

foreach ($path in $paths) {
  if (Test-Path -LiteralPath $path) {
    Remove-Item -LiteralPath $path -Recurse -Force
  }
}
```

Then run the full build command.

## PDF-Only Package

The installer is built from a staged application tree, not directly from
`C:\CraftRoot`.

Always prepare a fresh stage before a release installer. `-SkipStage` is only
for the final installer step after `prepare-okular-pdf-only-stage.ps1` and the
smoke test have already passed.

Prepare the stage:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\prepare-okular-pdf-only-stage.ps1
```

Smoke test the stage with a local PDF:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\smoke-test-okular-pdf-only-stage.ps1 `
  -PdfPath .\autotests\data\file2.pdf
```

Build the installer after the smoke test passes:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 -SkipStage
```

The stage keeps only the PDF generator. It must still include the runtime pieces
needed for opening and saving local PDFs, including:

- `bin\scholia.exe`
- `bin\kioworker.exe`
- `bin\Okular6Core.dll`
- `plugins\kf6\parts\okularpart.dll`
- `plugins\kf6\kio\kio_file.dll`
- `plugins\okular_generators\okularGenerator_poppler.dll`
- `bin\data\scholia`, including `bin\data\scholia\microtex\res`
- `share\poppler`

## TeX And LaTeX Notes

The Windows build supports two LaTeX note renderers:

- System TeX, discovered from the user's normal `PATH` or configured executable.
- MicroTeX, built from `external\MicroTeX` and shipped with Scholia resources.

This repository does not require or install a bundled TeX distribution.

For system-TeX rendering, Scholia expects `xelatex.exe` or `lualatex.exe` and
`pdfcrop.exe` to be available. The renderer compiles a source appearance once on
an A0 page with page numbers disabled, then uses `pdfcrop` to crop the result to
the actual drawn content. This replaces the old overflow-compensation behavior
that ran TeX a second time. `Overfull \hbox` remains a warning shown by Scholia;
it is not a reason to compile twice.

MicroTeX rendering does not require `pdfcrop.exe` or a system TeX distribution.
If Scholia is in Auto mode and no system TeX executable is found, it falls back
to MicroTeX when the runtime was built with `OKULAR_ENABLE_MICROTEX=ON`.
Scholia uses MicroTeX's text-mode entry point for note fallback rendering. Plain
text is parsed outside math mode; `$...`, `$$...$$`, `\(...\)`, and `\[...\]`
enter math mode explicitly. The forked MicroTeX text mode follows the
single-paragraph behavior expected for notes: `\textbf{...}` and
`\textit{...}` use TeX text fonts, consecutive spaces collapse to one space,
and line breaks are treated as one inter-word space.

To log TeX rendering calls, start Scholia with:

```powershell
$env:QT_LOGGING_RULES = 'org.jairy.scholia.ui.debug=true'
C:\CraftRoot\bin\scholia.exe
```

The persistent log file is:

```powershell
Get-Content "$env:LOCALAPPDATA\scholia\scholia-tex-debug.log" -Tail 80
```

Expected log operations:

- `system-latex` for one `xelatex` or `lualatex` compile.
- `texlive-pdfcrop` for system-TeX PDF cropping.
- `microtex-render` for MicroTeX rendering or Auto fallback.

To test the MicroTeX fallback without changing the machine-wide environment,
start Scholia from a PowerShell session that removes TeX distributions from
`PATH` but keeps Craft available:

```powershell
$env:QT_LOGGING_RULES = 'org.jairy.scholia.ui.debug=true'
$craftBin = 'C:\CraftRoot\bin'
$env:PATH = ($env:PATH -split ';' |
  Where-Object {
    $_ -and
    $_ -notmatch 'texlive' -and
    $_ -notmatch 'MiKTeX' -and
    $_ -notmatch 'TinyTeX'
  }) -join ';'
$env:PATH = "$craftBin;$env:PATH"

& 'C:\CraftRoot\bin\scholia.exe'
```

With system TeX hidden, the log should contain `microtex-render` and should not
contain `system-latex` or `texlive-pdfcrop`.

## Notes

- Use the wrapper scripts instead of running bare `ninja` from a normal shell;
  the wrappers enter the Craft/MSVC environment.
- Use `-PdfOnly` for both test builds and package builds.
- Use `-MicroTeXSrc .\external\MicroTeX` when building a Windows runtime that
  should include MicroTeX.
- Do not copy the whole `C:\CraftRoot\bin` tree into the package stage.
