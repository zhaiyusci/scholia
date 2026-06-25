# PDF-Only Packaging Checklist

Use this after the standalone SDK has been built.

## Build And Deploy

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-scholia-standalone.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64

powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\deploy-scholia-standalone-runtime.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

Verify the installed runtime:

```powershell
Get-Item ..\windows_build\install\scholia\bin\scholia.exe,
         ..\windows_build\install\scholia\bin\Okular6Core.dll,
         ..\windows_build\install\scholia\bin\plugins\kf6\parts\okularpart.dll,
         ..\windows_build\install\scholia\bin\plugins\okular_generators\okularGenerator_poppler.dll,
         ..\windows_build\install\scholia\bin\poppler-qt6.dll |
  Select-Object FullName,Length,LastWriteTime
```

## Stage

The installer script stages from the standalone install tree:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64 `
  -SkipBuild `
  -SkipDeploy
```

For a fast stage-only refresh while testing, skip the installer compression:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64 `
  -SkipBuild `
  -SkipDeploy `
  -SkipInstaller
```

The stage must include the PDF runtime:

- `bin\scholia.exe`
- `bin\Okular6Core.dll`
- `bin\plugins\kf6\parts\okularpart.dll`
- `bin\plugins\kf6\kio\kio_file.dll`
- `bin\plugins\okular_generators\okularGenerator_poppler.dll`
- `bin\data\scholia`
- `bin\share\poppler` or `share\poppler`, depending on the installed SDK data layout

Check generator trimming:

```powershell
Get-ChildItem ..\windows_build\dist\scholia-pdf\app\bin\plugins\okular_generators
```

Only `okularGenerator_poppler.dll` should be present.

## Smoke Test

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\smoke-test-okular-pdf-only-stage.ps1 `
  -PdfPath .\autotests\data\file2.pdf
```

The smoke test must launch from the stage and must not load project-local
runtime modules from outside the stage.

## Installer

If the stage has already passed the smoke test:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-okular-pdf-only-installer.ps1 `
  -SkipStage
```

The installer is written to:

```text
..\windows_build\dist
```
