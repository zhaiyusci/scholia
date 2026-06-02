# Okular Windows Build

This directory contains the Windows build and packaging scripts for this Okular
checkout. Run the commands below from the repository root.

## Layout

- Okular source: repository root, `.`
- Poppler source: `external\poppler`
- MicroTeX source: `external\MicroTeX`
- Craft root: `C:\CraftRoot`
- Okular build dir: `C:\CraftRoot\build\kde\applications\okular\work\build`
- Installed runtime:
  - `C:\CraftRoot\bin\okular.exe`
  - `C:\CraftRoot\bin\Okular6Core.dll`
  - `C:\CraftRoot\plugins\kf6\parts\okularpart.dll`
  - `C:\CraftRoot\plugins\okular_generators\okularGenerator_poppler.dll`
- Build logs: `..\build-logs`
- Package output: `..\dist`

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

This is the normal full Windows build for testing `C:\CraftRoot\bin\okular.exe`.
It builds local Poppler first, then builds Okular with only the PDF generator and
with MicroTeX enabled.

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 `
  -PdfOnly `
  -MicroTeXSrc .\external\MicroTeX
```

Use this after changing dependencies, Poppler, CMake options, or when the build
directory should be treated as untrusted.

To rebuild Okular from scratch while reusing the already installed local
Poppler:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 `
  -SkipLocalPoppler `
  -PdfOnly `
  -MicroTeXSrc .\external\MicroTeX
```

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
Get-Item C:\CraftRoot\bin\okular.exe,
         C:\CraftRoot\bin\Okular6Core.dll,
         C:\CraftRoot\plugins\kf6\parts\okularpart.dll,
         C:\CraftRoot\plugins\okular_generators\okularGenerator_poppler.dll,
         C:\CraftRoot\bin\poppler-qt6.dll |
  Select-Object FullName,Length,LastWriteTime

Test-Path C:\CraftRoot\bin\data\okular\microtex\res
```

Run the build:

```powershell
C:\CraftRoot\bin\okular.exe
```

## Incremental Okular Rebuild

Use this for ordinary source edits in Okular when the Craft build directory is
valid. It rebuilds the requested source files, relinks `okularpart.dll`, and
installs it into Craft.

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okularpart-incremental.ps1 `
  -SourceFiles part\latexrenderer.cpp
```

Pass every changed `.cpp` that belongs to `okularpart`. If the change touches
CMake configuration, generated headers, resources, Poppler, or MicroTeX, use the
full build instead.

## Clean Okular Build Directory

Use this only when a full Okular rebuild is needed. It removes Okular's Craft
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

- `bin\okular.exe`
- `bin\kioworker.exe`
- `bin\Okular6Core.dll`
- `plugins\kf6\parts\okularpart.dll`
- `plugins\kf6\kio\kio_file.dll`
- `plugins\okular_generators\okularGenerator_poppler.dll`
- `bin\data\okular`, including `bin\data\okular\microtex\res`
- `share\poppler`

## TeX And LaTeX Notes

The Windows build supports two LaTeX note renderers:

- System TeX, discovered from the user's normal `PATH` or configured executable.
- MicroTeX, built from `external\MicroTeX` and shipped with Okular resources.

This repository does not require or install a bundled TeX distribution.

For system-TeX rendering, Okular expects `xelatex.exe` or `lualatex.exe` and
`pdfcrop.exe` to be available. The renderer compiles a source appearance once on
an A0 page with page numbers disabled, then uses `pdfcrop` to crop the result to
the actual drawn content. This replaces the old overflow-compensation behavior
that ran TeX a second time. `Overfull \hbox` remains a warning shown by Okular;
it is not a reason to compile twice.

MicroTeX rendering does not require `pdfcrop.exe` or a system TeX distribution.
If Okular is in Auto mode and no system TeX executable is found, it falls back
to MicroTeX when the runtime was built with `OKULAR_ENABLE_MICROTEX=ON`.

To log TeX rendering calls, start Okular with:

```powershell
$env:QT_LOGGING_RULES = 'org.kde.okular.ui.debug=true'
C:\CraftRoot\bin\okular.exe
```

The persistent log file is:

```powershell
Get-Content "$env:LOCALAPPDATA\okular\okular-tex-debug.log" -Tail 80
```

Expected log operations:

- `system-latex` for one `xelatex` or `lualatex` compile.
- `texlive-pdfcrop` for system-TeX PDF cropping.
- `microtex-render` for MicroTeX rendering or Auto fallback.

To test the MicroTeX fallback without changing the machine-wide environment,
start Okular from a PowerShell session that removes TeX distributions from
`PATH` but keeps Craft available:

```powershell
$env:QT_LOGGING_RULES = 'org.kde.okular.ui.debug=true'
$craftBin = 'C:\CraftRoot\bin'
$env:PATH = ($env:PATH -split ';' |
  Where-Object {
    $_ -and
    $_ -notmatch 'texlive' -and
    $_ -notmatch 'MiKTeX' -and
    $_ -notmatch 'TinyTeX'
  }) -join ';'
$env:PATH = "$craftBin;$env:PATH"

& 'C:\CraftRoot\bin\okular.exe'
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
