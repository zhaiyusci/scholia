[CmdletBinding()]
param(
    [string] $CraftRoot = "C:\CraftRoot",
    [string] $StageRoot = "",
    [string] $DumpBin = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe",
    [switch] $SkipWinDeployQt
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$workspaceRoot = Split-Path -Parent $repoRoot
$windowsBuildRoot = Join-Path $workspaceRoot "windows_build"
if (!$StageRoot) {
    $StageRoot = Join-Path $windowsBuildRoot "dist\scholia-pdf\app"
}

$resolvedStageRoot = [System.IO.Path]::GetFullPath($StageRoot)
$allowedRoot = [System.IO.Path]::GetFullPath((Join-Path $windowsBuildRoot "dist"))
if (!$resolvedStageRoot.StartsWith($allowedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to recreate stage outside $allowedRoot`: $resolvedStageRoot"
}

if (Test-Path -LiteralPath $resolvedStageRoot) {
    Remove-Item -LiteralPath $resolvedStageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $resolvedStageRoot | Out-Null

$notesDir = Join-Path (Split-Path -Parent $PSScriptRoot) "manifests"
New-Item -ItemType Directory -Force -Path $notesDir | Out-Null
$requiredComponentsPath = Join-Path $notesDir "required-components.txt"
$dependencyScanPath = Join-Path $notesDir "dependency-scan.json"

$copied = [ordered]@{}

function Copy-FileFromCraft([string] $RelativePath, [string] $Reason) {
    $from = Join-Path $CraftRoot $RelativePath
    $to = Join-Path $resolvedStageRoot $RelativePath
    if (!(Test-Path -LiteralPath $from)) {
        throw "Missing required file: $from"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $to) | Out-Null
    Copy-Item -LiteralPath $from -Destination $to -Force
    $copied[$RelativePath] = $Reason
}

function Copy-DirectoryFromCraft([string] $RelativePath, [string] $Reason) {
    $from = Join-Path $CraftRoot $RelativePath
    $to = Join-Path $resolvedStageRoot $RelativePath
    if (!(Test-Path -LiteralPath $from)) {
        return
    }
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    & robocopy.exe $from $to /E /R:2 /W:1 /NFL /NDL /NP | Out-Host
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy failed for $from -> $to with exit code $LASTEXITCODE"
    }
    $copied[$RelativePath] = $Reason
}

function Get-ImportDlls([string] $Path) {
    if (!(Test-Path -LiteralPath $DumpBin)) {
        throw "Cannot find dumpbin: $DumpBin"
    }

    $lines = & $DumpBin /DEPENDENTS $Path 2>$null
    $inBlock = $false
    foreach ($line in $lines) {
        if ($line -match "Image has the following dependencies:") {
            $inBlock = $true
            continue
        }
        if (!$inBlock) {
            continue
        }
        if ($line -match "^\s*$") {
            continue
        }
        if ($line -match "^\s+(?<dll>[A-Za-z0-9_.+-]+\.dll)\s*$") {
            $matches.dll
        } elseif ($line -match "Summary") {
            break
        }
    }
}

function Find-StagedDll([string] $DllName) {
    Get-ChildItem -LiteralPath $resolvedStageRoot -Recurse -File -Filter $DllName -ErrorAction SilentlyContinue |
        Select-Object -First 1
}

function Find-CraftDll([string] $DllName) {
    $candidate = Join-Path (Join-Path $CraftRoot "bin") $DllName
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }
    return $null
}

$systemDlls = @(
    "ADVAPI32.dll", "AUTHZ.dll", "COMCTL32.dll", "COMDLG32.dll", "CRYPT32.dll",
    "DWMAPI.dll", "GDI32.dll", "IMM32.dll", "KERNEL32.dll", "MPR.dll",
    "NETAPI32.dll", "OLE32.dll", "OLEAUT32.dll", "PROPSYS.dll", "RPCRT4.dll",
    "SHELL32.dll", "SHLWAPI.dll", "USER32.dll", "USERENV.dll", "UXTHEME.dll",
    "VERSION.dll", "WINMM.dll", "WINTRUST.dll", "WS2_32.dll", "WTSAPI32.dll",
    "AVRT.dll", "bcrypt.dll", "d3d9.dll", "d3d11.dll", "d3d12.dll", "DNSAPI.dll", "DWrite.dll",
    "dxgi.dll", "IPHLPAPI.DLL", "msvcp140.dll", "msvcp140_1.dll",
    "msvcp140_2.dll", "MSWSOCK.dll", "ncrypt.dll", "Secur32.dll", "SETUPAPI.dll",
    "UIAutomationCore.DLL", "ucrtbase.dll", "vcruntime140.dll", "vcruntime140_1.dll", "WINHTTP.dll"
)
$systemDllLookup = @{}
foreach ($dll in $systemDlls) {
    $systemDllLookup[$dll.ToLowerInvariant()] = $true
}

function Test-SystemDll([string] $DllName) {
    $lowerDll = $DllName.ToLowerInvariant()
    if ($systemDllLookup.ContainsKey($lowerDll)) {
        return $true
    }

    return $lowerDll -like "api-ms-win-*.dll" -or
        $lowerDll -like "ext-ms-win-*.dll" -or
        $lowerDll -eq "ntdll.dll"
}

Write-Host "Preparing minimal PDF-only stage:"
Write-Host "  CraftRoot: $CraftRoot"
Write-Host "  StageRoot: $resolvedStageRoot"
Write-Host ""

Copy-FileFromCraft "bin\scholia.exe" "Application entry point"
Copy-FileFromCraft "bin\kioworker.exe" "KIO worker host executable required for file:// save/load operations"
Copy-FileFromCraft "bin\Okular6Core.dll" "Okular core library"
Copy-FileFromCraft "plugins\kf6\parts\okularpart.dll" "Scholia KParts UI plugin"
Copy-FileFromCraft "plugins\kf6\kio\kio_file.dll" "KIO file protocol worker required for file:// save/load operations"
Copy-FileFromCraft "plugins\okular_generators\okularGenerator_poppler.dll" "PDF generator"

Copy-DirectoryFromCraft "bin\data\okular" "Scholia annotation tools, stamps, and app data"
Copy-FileFromCraft "bin\data\applications\org.jairy.scholia.desktop" "Scholia shell desktop metadata"
Copy-FileFromCraft "bin\data\applications\okularApplication_pdf.desktop" "PDF-only application metadata"
Copy-FileFromCraft "bin\data\metainfo\org.kde.okular-poppler.metainfo.xml" "PDF generator metadata"
Copy-FileFromCraft "bin\data\metainfo\org.jairy.scholia.appdata.xml" "Scholia app metadata"
Copy-DirectoryFromCraft "bin\data\icons\hicolor" "Application and action icons"
Copy-DirectoryFromCraft "bin\data\mime" "MIME database used by KDE/Qt file type lookup"
Copy-DirectoryFromCraft "bin\data\kf6" "KF6 runtime service metadata"
Copy-DirectoryFromCraft "bin\data\qlogging-categories6" "Qt/KF6 logging category metadata"
Copy-DirectoryFromCraft "share\poppler" "Poppler CMap/CID data required by CJK and other encoded PDFs"

if (!$SkipWinDeployQt) {
    Write-Host ""
    Write-Host "Running windeployqt6 for Qt runtime..."
    & (Join-Path $CraftRoot "bin\windeployqt6.exe") `
        --release `
        --compiler-runtime `
        --no-translations `
        --no-system-d3d-compiler `
        --no-system-dxc-compiler `
        --dir (Join-Path $resolvedStageRoot "bin") `
        --plugindir (Join-Path $resolvedStageRoot "plugins") `
        (Join-Path $resolvedStageRoot "bin\scholia.exe")
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt6 failed with exit code $LASTEXITCODE"
    }
}

$qtConf = @"
[Paths]
Prefix=..
Plugins=plugins
Data=bin/data
"@
$qtConf | Set-Content -LiteralPath (Join-Path $resolvedStageRoot "bin\qt.conf") -Encoding ASCII
$copied["bin\qt.conf"] = "Qt path configuration for staged sibling plugin/data layout"

Write-Host ""
Write-Host "Resolving DLL imports from CraftRoot\\bin..."
$scanRows = @()
$scannedBinaries = @{}
$added = $true
while ($added) {
    $added = $false
    $scanRoots = @(
        (Join-Path $resolvedStageRoot "bin"),
        (Join-Path $resolvedStageRoot "plugins")
    ) | Where-Object { Test-Path -LiteralPath $_ }
    $binaries = foreach ($scanRoot in $scanRoots) {
        Get-ChildItem -LiteralPath $scanRoot -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in @(".exe", ".dll") }
    }
    $pendingBinaries = @($binaries | Where-Object { !$scannedBinaries.ContainsKey($_.FullName) })
    Write-Host "  scanning $($pendingBinaries.Count) new binaries..."
    foreach ($binary in $pendingBinaries) {
        $scannedBinaries[$binary.FullName] = $true
        foreach ($dll in Get-ImportDlls $binary.FullName) {
            if (Test-SystemDll $dll) {
                $scanRows += [pscustomobject]@{ Binary = $binary.FullName.Substring($resolvedStageRoot.Length + 1); Dependency = $dll; Action = "system" }
                continue
            }
            if (Find-StagedDll $dll) {
                $scanRows += [pscustomobject]@{ Binary = $binary.FullName.Substring($resolvedStageRoot.Length + 1); Dependency = $dll; Action = "already staged" }
                continue
            }
            $craftDll = Find-CraftDll $dll
            if ($craftDll) {
                $relative = "bin\$dll"
                Copy-FileFromCraft $relative "DLL import required by $($binary.Name)"
                $scanRows += [pscustomobject]@{ Binary = $binary.FullName.Substring($resolvedStageRoot.Length + 1); Dependency = $dll; Action = "copied from CraftRoot\\bin" }
                Write-Host "  + $dll  (needed by $($binary.Name))"
                $added = $true
            } else {
                $scanRows += [pscustomobject]@{ Binary = $binary.FullName.Substring($resolvedStageRoot.Length + 1); Dependency = $dll; Action = "missing" }
                Write-Warning "Missing non-system DLL: $dll needed by $($binary.FullName)"
            }
        }
    }
}

$manifestLines = @()
$manifestLines += "# Required components for Scholia PDF staging"
$manifestLines += "# Generated: $(Get-Date -Format o)"
$manifestLines += "# StageRoot: $resolvedStageRoot"
$manifestLines += ""
foreach ($entry in $copied.GetEnumerator() | Sort-Object Name) {
    $manifestLines += "$($entry.Key)`t$($entry.Value)"
}
$manifestLines | Set-Content -LiteralPath $requiredComponentsPath -Encoding UTF8
$scanRows | Sort-Object Binary, Dependency, Action | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $dependencyScanPath -Encoding UTF8

$generatorFiles = Get-ChildItem -LiteralPath (Join-Path $resolvedStageRoot "plugins\okular_generators") -File -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Name
$fileCount = (Get-ChildItem -LiteralPath $resolvedStageRoot -Recurse -File | Measure-Object).Count
$totalBytes = (Get-ChildItem -LiteralPath $resolvedStageRoot -Recurse -File | Measure-Object Length -Sum).Sum

Write-Host ""
Write-Host "Remaining Scholia generators:"
$generatorFiles | ForEach-Object { Write-Host "  $_" }
Write-Host ""
Write-Host "Stage file count: $fileCount"
Write-Host ("Stage size: {0:N1} MB" -f ($totalBytes / 1MB))
Write-Host "Required components: $requiredComponentsPath"
Write-Host "Dependency scan: $dependencyScanPath"
Write-Host "Stage ready: $resolvedStageRoot"
