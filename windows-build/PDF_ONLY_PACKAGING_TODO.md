# PDF-Only Packaging Checklist

Use this only after a successful Windows PDF-only build.

## Build

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-local.ps1 `
  -PdfOnly `
  -MicroTeXSrc .\external\MicroTeX
```

Verify:

```powershell
Select-String C:\CraftRoot\build\kde\applications\okular\work\build\CMakeCache.txt `
  -Pattern '^(OKULAR_PDF_ONLY|OKULAR_ENABLE_MICROTEX|MICROTEX_SRC):'
```

Expected:

```text
OKULAR_PDF_ONLY:BOOL=ON
OKULAR_ENABLE_MICROTEX:BOOL=ON
MICROTEX_SRC:PATH=<repo>/external/MicroTeX
```

## Stage

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\prepare-okular-pdf-only-stage.ps1
```

The stage must include the PDF runtime and omit non-PDF generators:

- `bin\scholia.exe`
- `bin\kioworker.exe`
- `bin\Okular6Core.dll`
- `plugins\kf6\parts\okularpart.dll`
- `plugins\kf6\kio\kio_file.dll`
- `plugins\okular_generators\okularGenerator_poppler.dll`
- `bin\data\scholia`
- `bin\data\scholia\microtex\res`
- `share\poppler`

Check generator trimming:

```powershell
Get-ChildItem ..\dist\okular-pdf-only\app\plugins\okular_generators
```

Only `okularGenerator_poppler.dll` should be present.

## Smoke Test

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\smoke-test-okular-pdf-only-stage.ps1 `
  -PdfPath .\autotests\data\file2.pdf
```

The smoke test must launch from the stage and must not load modules from
`C:\CraftRoot`.

## Installer

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 -SkipStage
```

The installer is written to `..\dist`.
