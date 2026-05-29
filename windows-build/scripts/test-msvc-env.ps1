[CmdletBinding()]
param(
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $VcVarsAll = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    [string] $Python = "C:\miniforge3\python.exe"
)

$ErrorActionPreference = "Stop"

$dumpenv = Join-Path $CraftRoot "craft\bin\dumpenv.py"
& $env:ComSpec /d /s /c "`"$VcVarsAll`" amd64 -vcvars_ver=14.3 > NUL && `"$Python`" `"$dumpenv`""
exit $LASTEXITCODE
