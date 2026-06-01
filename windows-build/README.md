# Okular Windows Native Build Notes

This workspace builds a native Windows Okular with KDE Craft/MSVC and a local
Poppler checkout.

The scripts in this directory are kept inside the Okular fork. The commands
below assume they are run from the repository root, so script paths are written
as `.\windows-build\...`.

## Layout

- Okular source: repository root, `.`
- Poppler source: sibling checkout or submodule, depending on the workflow
- Craft root: `C:\CraftRoot`
- Craft Okular source copy: avoid relying on this path; pass it explicitly if
  using an older Craft-managed source tree
- Craft Okular build dir: `C:\CraftRoot\build\kde\applications\okular\work\build`
- Installed Okular executable: `C:\CraftRoot\bin\okular.exe`
- Installed Okular part: `C:\CraftRoot\plugins\kf6\parts\okularpart.dll`
- TinyTeX: sibling workspace directory `..\TinyTeX`
- Logs: sibling workspace directory `..\build-logs`

## Dependencies

This build is not just a plain CMake build. The working Windows setup depends
on these pieces:

- Windows 10/11 x64.
- Visual Studio 2022 Build Tools with the MSVC C++ toolchain and Windows SDK.
- Git.
- KDE Craft installed at `C:\CraftRoot`, using the
  `windows-cl-msvc2022-x86_64` ABI.
- Local Okular checkout at the repository root.
- Local Poppler checkout either as `external\poppler` or as a sibling checkout,
  depending on the build script being used.
- Local Poppler installed into Craft, so Okular links against
  `C:\CraftRoot\bin\poppler-qt6.dll` and not a downloaded Craft cache package.
- Inno Setup 6 for building the installer. The current scripts expect:
  `C:\Users\Yu Zhai\AppData\Local\Programs\Inno Setup 6\ISCC.exe`
- A working `dumpbin.exe` from the Visual Studio tools. The staging script uses
  it to scan DLL imports.
- Qt deployment tools from Craft, especially:
  `C:\CraftRoot\bin\windeployqt6.exe`
- TinyTeX at `..\TinyTeX` for testing LaTeX note rendering. End users of
  the installer will still need a local TeX environment if they want to render
  new LaTeX notes.

Useful TeX packages installed in the current TinyTeX environment:

```text
standalone amsmath physics mhchem varwidth mathtools
```

The scripts are written for the paths above. Most scripts have parameters for
`WorkspaceRoot`, `CraftRoot`, `StageRoot`, or `OutputDir`, but the documented
working setup is the fixed layout in this file.

## Rules Of Thumb

- Use the local Poppler build. Do not let Craft silently replace it with a cache
  package when testing Poppler-dependent Okular behavior.
- Current Craft blueprints do not define a `local` version target for Poppler or
  Okular. Use the blueprint `srcDir` option for local source trees instead of
  `*.version=local`.
- When rebuilding local Poppler through Craft, pass `--no-cache`; otherwise
  Craft may satisfy `qt-libs/poppler` from the binary cache even when `srcDir`
  is set.
- Prefer incremental Ninja builds for Okular source edits. Avoid
  `craft --ignoreInstalled kde/applications/okular` unless a full package build
  is actually needed.
- Run scripts with visible output. For long commands, keep a log in
  `..\build-logs` and tail it from another PowerShell if needed.
- Remove `C:\Users\Yu Zhai\mingw64\bin` from `PATH` before entering Craft/MSVC;
  Craft warns when `gcc`, `g++`, or `cpp` are visible.
- If PowerShell blocks Craft scripts, run the wrapper with:
  `powershell.exe -ExecutionPolicy Bypass -NoProfile -File ...`

## Common Commands

Run a Craft command inside the configured environment:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\craft.ps1 --version
```

Build Okular with local Okular and local Poppler through Craft:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1
```

Build with only the PDF generator from source:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 -PdfOnly
```

Incrementally rebuild only changed Okular part files and install the new
`okularpart.dll`:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okularpart-incremental.ps1 `
  -SourceFiles part\latexnoteutils.cpp,part\annotationpopup.cpp,part\pageviewannotator.cpp
```

Run Okular:

```powershell
C:\CraftRoot\bin\okular.exe
```

## Local Poppler

The working build was made with local Poppler:

- Poppler branch: `local-okular-vector-stamp`
- Default source path: `external\poppler`
- Expected installed Qt DLL: `C:\CraftRoot\bin\poppler-qt6.dll`
- Okular PDF generator: `C:\CraftRoot\plugins\okular_generators\okularGenerator_poppler.dll`

Initialize the Poppler submodule before building:

```powershell
git submodule update --init --recursive external/poppler
```

The build wrapper uses the Craft `srcDir` option for both local source trees:

```text
--options qt-libs/poppler.srcDir=<repo>\external\poppler
--options kde/applications/okular.srcDir=<repo>
```

It also builds Poppler with `--no-cache --ignoreInstalled qt-libs/poppler`
before building Okular, so the installed `C:\CraftRoot\bin\poppler-qt6.dll`
comes from the submodule rather than from Craft's binary cache.

If you invoke Craft directly, the equivalent sequence is:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\craft.ps1 `
  --no-cache `
  --options qt-libs/poppler.srcDir=C:/path/to/okular/external/poppler `
  --ignoreInstalled qt-libs/poppler

powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\craft.ps1 `
  --no-cache `
  --options qt-libs/poppler.srcDir=C:/path/to/okular/external/poppler `
  --options kde/applications/okular.srcDir=C:/path/to/okular `
  --options kde/applications/okular.args=-DOKULAR_PDF_ONLY=ON `
  --ignoreInstalled kde/applications/okular
```

## PDF-Only Build And Packaging Plan

The repository now has a source-level switch:

```text
-DOKULAR_PDF_ONLY=ON
```

With that switch, `generators/CMakeLists.txt` builds only the Poppler PDF
generator and skips DjVu, DVI, EPUB, image, Markdown, MOBI, TIFF, XPS, and other
generators.

For Craft, pass that CMake option through the blueprint `args` setting:

```text
--options kde/applications/okular.args=-DOKULAR_PDF_ONLY=ON
```

Do not use `kde/applications/okular.configure.args` for this blueprint; it does
not set the CMake cache entry. Verify the configured build with:

```powershell
Select-String C:\CraftRoot\build\kde\applications\okular\work\build\CMakeCache.txt `
  -Pattern '^OKULAR_PDF_ONLY:'
```

The expected result is:

```text
OKULAR_PDF_ONLY:BOOL=ON
```

Important: do not package by copying the whole Craft root or the whole
`C:\CraftRoot\bin` tree. That pulls in Python, build tools, debug files,
unused Qt modules, and non-PDF dependencies. It produced a multi-GB staging
tree and a slow, oversized installer. That approach is only useful as a
temporary "does anything run at all" fallback, not as the distribution plan.

The correct packaging flow is:

1. Build Okular with `-DOKULAR_PDF_ONLY=ON`.
2. Create an empty staging directory.
3. Copy only the known entry points:
   - `bin\okular.exe`
   - `bin\Okular6Core.dll`
   - `plugins\kf6\parts\okularpart.dll`
   - `plugins\okular_generators\okularGenerator_poppler.dll`
   - `share\poppler`, especially the CMap/CID data required by CJK PDFs
   - the minimal Okular data files needed by the shell and part
4. Run `windeployqt6` on the staged `bin\okular.exe` to copy Qt runtime DLLs
   and Qt platform/image/icon plugins.
5. Recursively scan the staged EXE/DLL import tables and copy missing DLLs from
   `C:\CraftRoot\bin`.
6. Smoke test from the staging directory, not from `C:\CraftRoot`:
   - temporarily prepend only `stage\bin` to `PATH`
   - launch `stage\bin\okular.exe`
   - open a PDF
   - verify the LaTeX note workflow still renders and stores a PDF-backed stamp
   - verify non-PDF formats are not offered through Okular generators
7. Repeat dependency scan and smoke test until the staged app runs without
   falling back to `C:\CraftRoot`.
8. Only then build the Inno installer from the staged directory.

The smoke test is the gate. If a DLL is present only because the whole Craft
tree was copied, it should be treated as suspect until the staged app proves it
needs it.

Current smoke-tested staging:

```text
..\dist\okular-pdf-only\app
```

It opened the Makri PDF with a controlled PATH that did not include CraftRoot.
Loaded PDF-related staged modules were:

```text
bin\poppler.dll
bin\poppler-qt6.dll
plugins\kf6\parts\okularpart.dll
plugins\okular_generators\okularGenerator_poppler.dll
```

Saving local PDFs requires KIO's file worker as well:

```text
bin\kioworker.exe
plugins\kf6\kio\kio_file.dll
```

Without these two files, Okular can open PDFs but saving annotations to a
`file://` URL fails with `Unable to create KIO worker. Unknown protocol 'file'.`

No loaded module came from `C:\CraftRoot`. The staged generator directory
contains only `okularGenerator_poppler.dll`.

The generated component manifests are:

```text
windows-build\manifests\required-components.txt
windows-build\manifests\dependency-scan.json
windows-build\manifests\pdf-only-smoke-result.md
```

The current staged size is about 236.8 MB with 1390 files.

Expected PDF-specific runtime pieces include:

- `poppler-qt6.dll`
- `poppler.dll`
- `share\poppler\cMap`, `share\poppler\cidToUnicode`, and related Poppler
  data files for CJK and other encoded PDFs
- `freetype.dll`
- `zlib1.dll`, `libpng16.dll`, `jpeg62.dll`, `openjp2.dll`, and related Poppler
  image/font libraries discovered by the import scan

These are examples, not a hand-maintained final list. The final list should be
generated by dependency scanning plus smoke testing.

Current script status:

- `scripts\build-okular-local.ps1 -PdfOnly` is the intended source build entry.
  It builds `external\poppler` first unless `-SkipLocalPoppler` is passed; with
  `-SkipLocalPoppler`, Okular links against the Poppler already installed in
  Craft.
- `scripts\prepare-okular-pdf-only-stage.ps1` now uses the minimal staging flow
  and writes the component manifests.
- `scripts\smoke-test-okular-pdf-only-stage.ps1` verifies PDF loading and checks
  that no modules come from CraftRoot. Seeing
  `okularGenerator_poppler.dll` in the process module list is useful, but not a
  hard failure condition; depending on plugin loading timing, the staged app can
  open a PDF while the module snapshot only shows `poppler.dll`,
  `poppler-qt6.dll`, and `okularpart.dll`.
- `scripts\build-okular-pdf-only-installer.ps1` should be run only after the
  smoke test passes for the current staged tree.

Old broad staging command, kept here only as a warning:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\prepare-okular-pdf-only-stage.ps1
```

Final installer command, after the smoke test passes:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1
```

The installer always shows the destination directory page, so users can choose
where Okular PDF is installed. For unattended testing, Inno Setup also accepts a
destination path on the command line:

```powershell
.\Okular-PDF-26.07.70-patch20260530-Setup.exe /DIR="D:\Apps\Okular PDF"
```

The installer output is written to:

```text
..\dist
```

The latest updated installer is:

```text
..\dist\Okular-PDF-26.07.70-patch20260530-Setup.exe
```

The `patchYYYYMMDD` suffix is generated from the build date by default. Pass
`-Version` only when a release needs an explicit fixed name.

## LaTeX Note Sizing Fix

The Windows sizing bug was not a display rendering bug. The generated LaTeX PDF
used a real 10pt font, but the annotation rectangle was computed using
`Okular::Page::width()` as though it were PDF points.

For the Poppler generator, Okular pages are loaded as:

```text
PDF points / 72 * output DPI
```

So LaTeX note placement must convert back to PDF points:

```text
Okular page unit * 72 / output DPI
```

The fix was committed as:

```text
2b1f21f20 Fix LaTeX note physical sizing
```

Files involved:

- `part/latexnoteutils.cpp`
- `part/latexnoteutils.h`
- `part/pageviewannotator.cpp`
- `part/annotationpopup.cpp`

## TinyTeX

TinyTeX is installed in `..\TinyTeX`, with the user PATH entry:

```text
..\TinyTeX\bin\windows
```

Installed packages include:

```text
standalone amsmath physics mhchem varwidth mathtools
```

Install or refresh packages:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\install-tex-packages.ps1
```

## Useful Diagnostics

Check whether a PDF still contains stale local LaTeX appearance paths:

```powershell
rg -a -n "latex-notes|latexAppearancePdfFileName" "C:\path\to\file.pdf"
```

Check saved annotations in the test PDF:

```powershell
rg -a -n "/Subtype\s*/Stamp|latex-notes|/Rect|/AP" "C:\Users\Yu Zhai\Downloads\Makri_2023_J._Phys._A__Math._Theor._56_144001.pdf"
```

For the fixed case, the saved stamp `/Rect` height should be in PDF points and
should no longer be shrunk by the screen DPI ratio.
