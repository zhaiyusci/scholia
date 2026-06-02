param(
    [string] $OutDir = ""
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (!$OutDir) {
    $OutDir = Join-Path $scriptRoot "out"
}

$texFile = Join-Path $scriptRoot "cjk-xelatex-smoke.tex"
$xelatex = (Get-Command xelatex.exe -ErrorAction SilentlyContinue).Source
$kpsewhich = (Get-Command kpsewhich.exe -ErrorAction SilentlyContinue).Source

if (!$xelatex) {
    throw "xelatex.exe was not found in PATH."
}
if (!$kpsewhich) {
    throw "kpsewhich.exe was not found in PATH."
}

Write-Host "Checking required XeLaTeX CJK package..."
& $kpsewhich xeCJK.sty | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "xeCJK.sty was not found. Install the xeCJK/ctex packages in the system TeX distribution."
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
