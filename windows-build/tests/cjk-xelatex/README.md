# XeLaTeX Chinese PDF Smoke Test

This test creates a small Chinese PDF with XeLaTeX. It is intended to verify
that a local TeX environment can produce Chinese text for Okular PDF rendering
tests.

Run:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\tests\cjk-xelatex\build-cjk-xelatex-smoke.ps1
```

Output:

```text
.\windows-build\tests\cjk-xelatex\out\cjk-xelatex-smoke.pdf
```

The script checks for `xeCJK.sty`; if it is missing, it installs the relevant
packages with your system TeX package manager before running this smoke test.
