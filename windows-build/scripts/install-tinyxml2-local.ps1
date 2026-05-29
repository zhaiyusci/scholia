param(
    [string] $TinyXml2Source = "",
    [string] $BuildDir = "",
    [string] $CraftRootPath = "C:\CraftRoot"
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$workspaceRoot = Split-Path -Parent $repoRoot
if (!$TinyXml2Source) {
    $TinyXml2Source = Join-Path $workspaceRoot "tinyxml2"
}
if (!$BuildDir) {
    $BuildDir = Join-Path $workspaceRoot "tinyxml2-build-craft"
}

$resolvedCraftRoot = (Resolve-Path -LiteralPath $CraftRootPath).Path

& (Join-Path $resolvedCraftRoot "craft\craftenv.ps1")

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

& (Join-Path $resolvedCraftRoot "dev-utils\bin\cmake.exe") `
    -S $TinyXml2Source `
    -B $BuildDir `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=RelWithDebInfo `
    "-DCMAKE_INSTALL_PREFIX=${resolvedCraftRoot}" `
    -DBUILD_SHARED_LIBS=OFF `
    -Dtinyxml2_BUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& (Join-Path $resolvedCraftRoot "dev-utils\bin\cmake.exe") `
    --build $BuildDir `
    --config RelWithDebInfo `
    --target install
exit $LASTEXITCODE
