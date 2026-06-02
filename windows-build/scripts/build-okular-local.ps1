[CmdletBinding()]
param(
    [string] $WorkspaceRoot = "",
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $OkularSrc = "",
    [string] $PopplerSrc = "",
    [string] $OkularVersion = "",
    [string] $PopplerVersion = "",
    [string] $MicroTeXSrc = "",
    [switch] $SkipLocalPoppler,
    [switch] $PdfOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (!$WorkspaceRoot) {
    $WorkspaceRoot = Split-Path -Parent $repoRoot
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)
if (!$OkularSrc) {
    $OkularSrc = $repoRoot
}
if (!$PopplerSrc) {
    $PopplerSrc = Join-Path $repoRoot "external\poppler"
}

function Convert-ToCraftPath([string] $Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    return $resolved -replace "\\", "/"
}

function Invoke-Craft([string[]] $CraftArgs) {
    & (Join-Path $PSScriptRoot "craft.ps1") @CraftArgs | ForEach-Object { Write-Host $_ }
    return $LASTEXITCODE
}

$logDir = Join-Path $WorkspaceRoot "build-logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $logDir "okular-build-$stamp.log"

Start-Transcript -Path $logPath -Force | Out-Null
try {
    Write-Host "Okular local build started at $(Get-Date -Format o)"
    Write-Host "Log: $logPath"
    Write-Host "Craft root: $CraftRoot"
    Write-Host "Okular source: $OkularSrc"
    Write-Host "Poppler source: $PopplerSrc"
    Write-Host "Okular version override: $OkularVersion"
    Write-Host "Poppler version override: $PopplerVersion"
    Write-Host "PDF-only generator build: $PdfOnly"
    Write-Host "MicroTeX source: $MicroTeXSrc"
    Write-Host ""

    $craftOptions = @()
    if ($OkularSrc) {
        $craftOptions += @("--options", "kde/applications/okular.srcDir=$(Convert-ToCraftPath $OkularSrc)")
    }
    if ($PopplerSrc -and !$SkipLocalPoppler) {
        $craftOptions += @("--options", "qt-libs/poppler.srcDir=$(Convert-ToCraftPath $PopplerSrc)")
    }
    if ($OkularVersion) {
        $craftOptions += @("--options", "kde/applications/okular.version=$OkularVersion")
    }
    if ($PopplerVersion) {
        $craftOptions += @("--options", "qt-libs/poppler.version=$PopplerVersion")
    }

    $okularCMakeArgs = @()
    if ($PdfOnly) {
        $okularCMakeArgs += "-DOKULAR_PDF_ONLY=ON"
    }
    if ($MicroTeXSrc) {
        $resolvedMicroTeXSrc = (Resolve-Path -LiteralPath $MicroTeXSrc).Path
        $resolvedMicroTeXSrc = $resolvedMicroTeXSrc -replace "\\", "/"
        $okularCMakeArgs += "-DOKULAR_ENABLE_MICROTEX=ON"
        $okularCMakeArgs += "-DMICROTEX_SRC=$resolvedMicroTeXSrc"
    }
    if ($okularCMakeArgs.Count -gt 0) {
        $craftOptions += @("--options", "kde/applications/okular.args=$($okularCMakeArgs -join ' ')")
    }

    if (!$SkipLocalPoppler) {
        if (!(Test-Path -LiteralPath $PopplerSrc)) {
            throw "Missing local Poppler source: $PopplerSrc. Run git submodule update --init --recursive first."
        }
        if (!(Test-Path -LiteralPath (Join-Path $PopplerSrc "CMakeLists.txt"))) {
            throw "Local Poppler source is not initialized: $PopplerSrc. Run git submodule update --init --recursive first."
        }

        Write-Host "Building local Poppler from source..."
        $popplerArgs = @("--no-cache") + $craftOptions + @("--ignoreInstalled", "qt-libs/poppler")
        $popplerExitCode = Invoke-Craft $popplerArgs
        if ($popplerExitCode -ne 0) {
            Write-Host ""
            Write-Host "Local Poppler build failed with code $popplerExitCode at $(Get-Date -Format o)"
            exit $popplerExitCode
        }
        Write-Host ""
    }

    Write-Host "Building Okular from source..."
    $okularArgs = @("--no-cache") + $craftOptions + @("--ignoreInstalled", "kde/applications/okular")
    $exitCode = Invoke-Craft $okularArgs
    Write-Host ""
    Write-Host "Craft exited with code $exitCode at $(Get-Date -Format o)"
    exit $exitCode
}
finally {
    Stop-Transcript | Out-Null
}
