# Scholia KF6 SDK Bootstrap

This path builds the KF6 development SDK used by the Windows standalone Scholia
build. The default version is the stable KF6 tag `v6.27.0`.

All output goes under the sibling workspace:

```text
..\windows_build\sources
..\windows_build\build\kf6-sdk
..\windows_build\sdk
```

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

The script writes `scholia-sdk-bootstrap.json` into the SDK prefix so later
build steps reuse the same Qt, CMake, Ninja, and MSVC paths.

## Support Libraries

Build zlib and freetype before modules that need compression or font support:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-zlib-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile -File .\windows-build\scripts\build-freetype-sdk.ps1
```

Install the libintl compatibility shim used by KI18n on Windows:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\install-gettext-native-sdk.ps1
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-libintl-shim-sdk.ps1
```

The native gettext install provides the real Windows `msgfmt.exe` used to
compile `.mo` catalogs. The libintl shim exports the gettext symbols needed by
KI18n and still provides minimal fallback tools, but it is not a full gettext
replacement.

Install pinned flex and bison wrappers for modules that generate parsers:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\install-winflexbison-sdk.ps1
```

## Build KF6 Modules

After ECM is installed, build individual stable KF6 modules with:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-kf6-module.ps1 `
  -Module kcoreaddons
```

The script records installed modules in:

```text
..\windows_build\sdk\scholia-sdk-modules.json
```

Some modules need extra CMake options. For example, build `KArchive` with
nonessential compression and crypto backends disabled:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-kf6-module.ps1 `
  -Module karchive `
  -ExtraCMakeArgs -DWITH_BZIP2=OFF,-DWITH_LIBLZMA=OFF,-DWITH_OPENSSL=OFF,-DWITH_LIBZSTD=OFF
```

## Build Poppler

After zlib and freetype are installed in the SDK, build custom Poppler:

```powershell
powershell.exe -ExecutionPolicy Bypass -NoProfile `
  -File .\windows-build\scripts\build-poppler-sdk.ps1 `
  -QtPrefix C:\Qt\6.11.1\msvc2022_64
```

Scholia uses the installed Poppler libraries from `..\windows_build\sdk` and
the generated private Poppler headers from
`..\windows_build\build\thirdparty\poppler-custom`.
