# PDF-Only Stage Smoke Result

This file may be regenerated after running:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\smoke-test-okular-pdf-only-stage.ps1 `
  -PdfPath .\autotests\data\file2.pdf
```

Expected result:

- The staged `bin\scholia.exe` starts.
- The PDF opens.
- No project-local runtime module is loaded from outside the stage tree.
- The staged generator directory contains only `okularGenerator_poppler.dll`.

Record the concrete smoke-test output here when preparing a release package.
