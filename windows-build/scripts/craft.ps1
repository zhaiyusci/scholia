[CmdletBinding(PositionalBinding = $false)]
param(
    [string] $CraftRoot = "C:\CraftRoot",

    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $CraftArgs
)

$ErrorActionPreference = "Stop"

$mingwPath = "C:\Users\Yu Zhai\mingw64\bin"
$env:Path = (($env:Path -split ";") | Where-Object { $_ -and ($_ -ne $mingwPath) }) -join ";"

$craftEnv = Join-Path $CraftRoot "craft\craftenv.ps1"
$env:LESS = "x"
$oldErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "SilentlyContinue"
& $craftEnv
$ErrorActionPreference = $oldErrorActionPreference
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& craft @CraftArgs
exit $LASTEXITCODE
