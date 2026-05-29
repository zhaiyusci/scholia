param(
    [string] $TinyTeXRoot = "",
    [string] $OutDir = ""
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptRoot "..\..\.."))
$workspaceRoot = Split-Path -Parent $repoRoot
if (!$TinyTeXRoot) {
    $TinyTeXRoot = Join-Path $workspaceRoot "TinyTeX"
}
if (!$OutDir) {
    $OutDir = Join-Path $scriptRoot "out"
}

$texFile = Join-Path $scriptRoot "cjk-xelatex-smoke.tex"
$tinyTeXBin = Join-Path $TinyTeXRoot "bin\windows"
$xelatex = Join-Path $tinyTeXBin "xelatex.exe"
$tlmgr = Join-Path $tinyTeXBin "tlmgr.bat"

if (!(Test-Path -LiteralPath $xelatex)) {
    throw "xelatex.exe not found: $xelatex"
}
if (!(Test-Path -LiteralPath $tlmgr)) {
    throw "tlmgr.bat not found: $tlmgr"
}

$env:PATH = "$tinyTeXBin;$env:PATH"

Write-Host "Checking required XeLaTeX CJK package..."
& kpsewhich.exe xeCJK.sty | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "xeCJK.sty not found. Installing TinyTeX packages: xecjk ctex cjk"
    & $tlmgr install xecjk ctex cjk
    if ($LASTEXITCODE -ne 0) {
        throw "tlmgr failed while installing CJK packages."
    }
} else {
    Write-Host "Found xeCJK.sty."
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Write-Host "Compiling Chinese XeLaTeX smoke PDF..."
& $xelatex `
    -interaction=nonstopmode `
    -halt-on-error `
    -file-line-error `
    -output-directory="$OutDir" `
    $texFile

if ($LASTEXITCODE -ne 0) {
    throw "xelatex failed."
}

$pdf = Join-Path $OutDir "cjk-xelatex-smoke.pdf"
if (!(Test-Path -LiteralPath $pdf)) {
    throw "Expected PDF was not generated: $pdf"
}

Write-Host "Generated: $pdf"
