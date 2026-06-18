# Scholia KF6 SDK Bootstrap

This path is for building Scholia with the local MSVC toolchain and a normal Qt
installation, without using Craft as the build driver or install tree.

The first step intentionally builds only ECM into a standalone SDK prefix. Later
steps can add the required KF6 modules to the same prefix. The default version
is the stable KF6 tag `v6.27.0`, not a development branch.

## Bootstrap ECM

From the repository root:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\bootstrap-kf6-sdk.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

To pin a different stable KF6 release:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\bootstrap-kf6-sdk.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64 `
  -Kf6Version 6.26.0
```

Default layout:

```text
..\windows_build\sources\extra-cmake-modules
..\windows_build\build\kf6-sdk\extra-cmake-modules
..\windows_build\sdk
```

After the bootstrap finishes, load the generated environment:

```powershell
. ..\windows_build\sdk\scholia-sdk-env.ps1
```

The script writes `scholia-sdk-bootstrap.json` into the SDK prefix so later build
steps can reuse the exact Qt, CMake, Ninja, and MSVC paths.

## Current Scope

This does not remove KF6 from Scholia. It only starts replacing Craft as the
thing that owns the build workspace and install prefix.

The intended next steps are:

1. Build the required KF6 modules into `..\windows_build\sdk`.
2. Build Poppler from `external\poppler` into the same SDK prefix.
3. Configure Scholia with `CMAKE_PREFIX_PATH=<QtPrefix>;..\windows_build\sdk`.
4. Update staging to copy Scholia artifacts from the standalone install prefix,
   using Craft only as a temporary fallback for DLL dependencies.

## Build One KF6 Module

After ECM is installed, build individual stable KF6 modules with:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-kf6-module.ps1 `
  -Module kcoreaddons
```

The script uses the same default stable version, `v6.27.0`, and records
installed modules in:

```text
..\windows_build\sdk\scholia-sdk-modules.json
```

Some KF6 modules need third-party C libraries. Build zlib into the SDK with:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-zlib-sdk.ps1
```

For example, `KArchive` can then be built with the nonessential compression and
crypto backends disabled:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-kf6-module.ps1 `
  -Module karchive `
  -ExtraCMakeArgs -DWITH_BZIP2=OFF,-DWITH_LIBLZMA=OFF,-DWITH_OPENSSL=OFF,-DWITH_LIBZSTD=OFF
```
