[CmdletBinding()]
param(
    [string] $WslDistro = "openSUSE-Tumbleweed",
    [string] $QtPrefix = "",
    [string] $SdkPrefix = "",
    [string] $WindowsBuildRoot = "",
    [string] $WindowsVersion = "",
    [string] $WindowsFileVersion = "",
    [string] $AppImageVersion = "",
    [string] $PackageOutputRoot = "",
    [string] $AppImageTool = "",
    [string] $AppImageRuntimeFile = "",
    [int] $WindowsJobs = 8,
    [switch] $DownloadAppImageTool,
    [switch] $DownloadAppImageRuntime,
    [switch] $SkipWindows,
    [switch] $SkipLinux,
    [switch] $SkipWindowsBuild,
    [switch] $SkipLinuxBuild,
    [switch] $SkipWindowsPackage,
    [switch] $SkipAppImagePackage
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$workspaceRoot = Split-Path -Parent $repoRoot

if (!$WindowsBuildRoot) {
    $WindowsBuildRoot = Join-Path $workspaceRoot "windows_build"
}
$WindowsBuildRoot = [System.IO.Path]::GetFullPath($WindowsBuildRoot)

if (!$AppImageVersion) {
    $AppImageVersion = Get-Date -Format "yyyyMMdd"
}
if (!$PackageOutputRoot) {
    $PackageOutputRoot = Join-Path $workspaceRoot "packages"
}
$PackageOutputRoot = [System.IO.Path]::GetFullPath($PackageOutputRoot)

$windowsDist = Join-Path $WindowsBuildRoot "dist"
$linuxAppImageDir = Join-Path $workspaceRoot "linux_build\appimage"
$results = [ordered]@{}
$copiedResults = [ordered]@{}

function Invoke-Step([string] $Name, [scriptblock] $Body) {
    Write-Host ""
    Write-Host "==> $Name" -ForegroundColor Cyan
    & $Body
    Write-Host "<== $Name" -ForegroundColor Green
}

function Invoke-External([string] $FilePath, [string[]] $Arguments) {
    Write-Host ("+ {0} {1}" -f $FilePath, ($Arguments -join " "))
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function ConvertTo-WslPath([string] $WindowsPath) {
    $fullPath = [System.IO.Path]::GetFullPath($WindowsPath)
    $drive = $fullPath.Substring(0, 1).ToLowerInvariant()
    $rest = $fullPath.Substring(2) -replace "\\", "/"
    return "/mnt/$drive$rest"
}

function Quote-Bash([string] $Value) {
    return "'" + ($Value -replace "'", "'\''") + "'"
}

function Get-NewestFile([string] $Directory, [string] $Filter) {
    if (!(Test-Path -LiteralPath $Directory)) {
        return $null
    }
    Get-ChildItem -LiteralPath $Directory -Filter $Filter -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

Write-Host "Scholia local package build"
Write-Host "  Repo:               $repoRoot"
Write-Host "  Windows build root: $WindowsBuildRoot"
Write-Host "  Linux AppImage dir: $linuxAppImageDir"
Write-Host "  Package output:     $PackageOutputRoot"
Write-Host "  WSL distro:         $WslDistro"
Write-Host "  AppImage version:   $AppImageVersion"

if (!$SkipWindows) {
    if (!$SkipWindowsBuild) {
        Invoke-Step "Windows standalone build" {
            $args = @(
                "-ExecutionPolicy", "Bypass",
                "-NoProfile",
                "-File", (Join-Path $repoRoot "windows-build\scripts\build-scholia-standalone.ps1"),
                "-WorkspaceRoot", $WindowsBuildRoot,
                "-Jobs", $WindowsJobs
            )
            if ($QtPrefix) {
                $args += @("-QtPrefix", $QtPrefix)
            }
            if ($SdkPrefix) {
                $args += @("-SdkPrefix", $SdkPrefix)
            }
            Invoke-External "powershell.exe" $args
        }
    }

    if (!$SkipWindowsPackage) {
        Invoke-Step "Windows installer" {
            $args = @(
                "-ExecutionPolicy", "Bypass",
                "-NoProfile",
                "-File", (Join-Path $repoRoot "windows-build\scripts\build-scholia-installer.ps1"),
                "-WorkspaceRoot", $WindowsBuildRoot,
                "-Jobs", $WindowsJobs,
                "-SkipBuild"
            )
            if ($QtPrefix) {
                $args += @("-QtPrefix", $QtPrefix)
            }
            if ($SdkPrefix) {
                $args += @("-SdkPrefix", $SdkPrefix)
            }
            if ($WindowsVersion) {
                $args += @("-Version", $WindowsVersion)
            }
            if ($WindowsFileVersion) {
                $args += @("-FileVersion", $WindowsFileVersion)
            }
            Invoke-External "powershell.exe" $args

            $installer = Get-NewestFile $windowsDist "Scholia-*-Setup.exe"
            if (!$installer) {
                throw "Windows installer was not found under $windowsDist"
            }
            $results["Windows installer"] = $installer.FullName
        }
    }
}

if (!$SkipLinux) {
    if (!$SkipAppImagePackage) {
        $repoWsl = ConvertTo-WslPath $repoRoot

        $linuxLines = @(
            "set -eu",
            "cd $(Quote-Bash $repoWsl)"
        )
        if (!$SkipLinuxBuild) {
            $linuxLines += "linux-build/scripts/build-okular-local.sh"
        }

        $appImageEnv = @("VERSION=$(Quote-Bash $AppImageVersion)")
        if ($AppImageTool) {
            $appImageEnv += "APPIMAGETOOL=$(Quote-Bash (ConvertTo-WslPath $AppImageTool))"
        }
        if ($AppImageRuntimeFile) {
            $appImageEnv += "APPIMAGE_RUNTIME_FILE=$(Quote-Bash (ConvertTo-WslPath $AppImageRuntimeFile))"
        }
        if ($DownloadAppImageTool) {
            $appImageEnv += "DOWNLOAD_APPIMAGETOOL=1"
        }
        if ($DownloadAppImageRuntime) {
            $appImageEnv += "DOWNLOAD_APPIMAGE_RUNTIME=1"
        }
        $linuxLines += (($appImageEnv + @("linux-build/scripts/build-okular-appimage.sh")) -join " ")

        Invoke-Step "Linux build and AppImage" {
            Invoke-External "wsl.exe" @("-d", $WslDistro, "--", "bash", "-lc", ($linuxLines -join " && "))
        }

        $appImage = Get-NewestFile $linuxAppImageDir "Okular-dev-$AppImageVersion-*.AppImage"
        if (!$appImage) {
            throw "AppImage was not found under $linuxAppImageDir"
        }
        $results["Linux AppImage"] = $appImage.FullName
    } elseif (!$SkipLinuxBuild) {
        Invoke-Step "Linux full build" {
            $repoWsl = ConvertTo-WslPath $repoRoot
            $linuxLines = @(
                "set -eu",
                "cd $(Quote-Bash $repoWsl)",
                "linux-build/scripts/build-okular-local.sh"
            )
            Invoke-External "wsl.exe" @("-d", $WslDistro, "--", "bash", "-lc", ($linuxLines -join " && "))
        }
    }
}

if ($results.Count -gt 0) {
    Invoke-Step "Collect package outputs" {
        New-Item -ItemType Directory -Force -Path $PackageOutputRoot | Out-Null
        foreach ($entry in $results.GetEnumerator()) {
            $source = $entry.Value
            $destination = Join-Path $PackageOutputRoot (Split-Path -Leaf $source)
            Copy-Item -LiteralPath $source -Destination $destination -Force
            $copiedResults[$entry.Key] = $destination
            Write-Host ("Copied {0}: {1}" -f $entry.Key, $destination)
        }
    }
}

Write-Host ""
Write-Host "Built package outputs:" -ForegroundColor Green
foreach ($entry in $results.GetEnumerator()) {
    Write-Host ("  {0}: {1}" -f $entry.Key, $entry.Value)
}

if ($copiedResults.Count -gt 0) {
    Write-Host ""
    Write-Host "Collected package outputs:" -ForegroundColor Green
    foreach ($entry in $copiedResults.GetEnumerator()) {
        Write-Host ("  {0}: {1}" -f $entry.Key, $entry.Value)
    }
}
