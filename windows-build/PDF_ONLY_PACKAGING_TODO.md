# PDF-Only Packaging TODO

Goal: produce a Windows installer that gives users a native Okular build with
PDF support only.

## Do Not Repeat

- Do not copy the whole `C:\CraftRoot\bin` tree into the package.
- Do not start Inno Setup before the staging directory has passed a smoke test.
- Do not treat "only `okularGenerator_poppler.dll` remains" as enough. That
  proves generator-level trimming only; it does not prove the runtime DLL set is
  minimal or correct.

## Source Build

Use the PDF-only CMake switch:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 -PdfOnly
```

This should configure Okular with:

```text
-DOKULAR_PDF_ONLY=ON
```

For Craft, the option must be passed as:

```text
--options kde/applications/okular.args=-DOKULAR_PDF_ONLY=ON
```

`kde/applications/okular.configure.args` does not set the CMake cache entry for
this blueprint. Always verify:

```text
OKULAR_PDF_ONLY:BOOL=ON
```

Expected generator output:

```text
plugins\okular_generators\okularGenerator_poppler.dll
```

No other `okularGenerator_*.dll` should be needed in the final package.

## Minimal Staging Algorithm

1. Recreate `..\dist\okular-pdf-only\app`.
2. Copy only first-order app files from `C:\CraftRoot`:
   - `bin\okular.exe`
   - `bin\kioworker.exe`
   - `bin\Okular6Core.dll`
   - `plugins\kf6\parts\okularpart.dll`
   - `plugins\kf6\kio\kio_file.dll`
   - `plugins\okular_generators\okularGenerator_poppler.dll`
3. Copy minimal Okular data files:
   - `bin\data\okular`
   - `bin\data\applications\org.kde.okular.desktop` or the installed equivalent
   - `bin\data\applications\okularApplication_pdf.desktop`
   - `bin\data\metainfo\org.kde.okular-poppler.metainfo.xml` if present
   - icons required by the shell
4. Run `windeployqt6 --release --compiler-runtime --no-translations` against
   staged `bin\okular.exe`. Add translations later only if needed.
5. Recursively scan imports for every staged `.exe` and `.dll`.
6. For missing DLLs, copy from `C:\CraftRoot\bin`.
7. Repeat until the scan is stable.
8. Smoke test from the staged tree.

## Smoke Test

Run with a controlled PATH:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\smoke-test-okular-pdf-only-stage.ps1 -PdfPath path\to\test.pdf
```

Check:

- Okular launches without depending on `C:\CraftRoot\bin` in PATH.
- The PDF opens.
- Annotation tools are present.
- LaTeX note renders.
- Saved LaTeX note stamp uses a correctly sized `/Rect`.
- Non-PDF generators are absent from the staged
  `plugins\okular_generators` directory.

Current smoke status:

- Passed from `..\dist\okular-pdf-only\app`.
- Staged Okular opened the Makri PDF.
- `okularGenerator_poppler.dll` was loaded from the staged tree.
- No loaded module came from `C:\CraftRoot`.
- Staged size after dependency scan: 1388 files, about 236.6 MB.
- See `windows-build\manifests\pdf-only-smoke-result.md`.

Save-path note:

- Saving annotations to a local PDF uses KIO even for `file://` URLs.
- If `bin\kioworker.exe` or `plugins\kf6\kio\kio_file.dll` is missing, Okular
  can open PDFs but saving fails with:
  `Unable to create KIO worker. Unknown protocol 'file'.`

## Installer Gate

Only run Inno Setup after the staging smoke test passes.

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 -SkipStage
```

The installer should be built from the verified staging directory, not from
CraftRoot.
