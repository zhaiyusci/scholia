# PDF-Only Staging Smoke Result

Last run: 2026-05-24 07:13 Asia/Shanghai

## Result

Pass.

The staged executable opened:

```text
C:\Users\Yu Zhai\Downloads\Makri_2023_J._Phys._A__Math._Theor._56_144001.pdf
```

The window title became:

```text
Electronic frustration, Berry's phase interference and slow dynamics in some tight-binding systems coupled to harmonic baths - Okular
```

The process loaded these PDF-related modules from the staged tree:

```text
..\dist\okular-pdf-only\app\bin\poppler.dll
..\dist\okular-pdf-only\app\bin\poppler-qt6.dll
..\dist\okular-pdf-only\app\plugins\kf6\parts\okularpart.dll
..\dist\okular-pdf-only\app\plugins\okular_generators\okularGenerator_poppler.dll
```

No loaded module path started with:

```text
C:\CraftRoot\
```

The staged generator directory contains only:

```text
okularGenerator_poppler.dll
```

## Staging Size

```text
Files: 1388
Bytes: 248101654
Approx: 236.6 MB
```

## Gotcha

When launching the smoke-test PDF through PowerShell, quote the PDF argument.
The path contains a space in `Yu Zhai`; without explicit quoting Okular opens
without loading the PDF generator.

Use:

```powershell
Start-Process -FilePath "$stage\bin\okular.exe" -ArgumentList "`"$pdf`""
```
