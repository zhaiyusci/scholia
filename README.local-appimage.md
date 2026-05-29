# Local AppImage build

This document packages the local Linux Okular install into an AppImage. It is
based on the same staging principle used by the Windows package: copy only the
runtime pieces that Okular needs, bundle Poppler's CMap/CID data, then run a
smoke test from the staged application instead of relying on the build prefix.

The AppImage scripts assume the local Linux build has already been installed
to:

```sh
PREFIX=$HOME/.local/opt/okular
```

Build that first using `README.local-linux-build.md`.

## Prepare an AppDir

```sh
cd /path/to/okular
PREFIX=$HOME/.local/opt/okular \
linux-build/scripts/prepare-okular-appdir.sh
```

The generated AppDir defaults to:

```sh
build-appimage/Okular.AppDir
```

The script copies:

- `usr/bin/okular`
- `libOkular6Core`, the Okular KParts UI plugin, and the PDF Poppler generator
- `usr/share/poppler`, including CMap/CID data needed by CJK PDFs
- selected Qt 6 platform/image/icon/network/print plugins
- selected KF6 plugins and runtime data needed for local file access
- Breeze icons, so toolbar icons are available without relying on the host

It does not copy the whole local prefix. In particular, Poppler command-line
tools, development files, non-PDF generator metadata, and Qt translations are
left out by default. Runtime shared libraries are discovered by recursively
running `ldd` over the staged ELF files, copying only the libraries that are
actually needed.

By default the AppDir is PDF-only and copies only the Poppler generator. To keep
all installed generators:

```sh
PDF_ONLY=0 linux-build/scripts/prepare-okular-appdir.sh
```

The default is intentionally conservative. It strips staged ELF binaries and
does not bundle Qt translations, Qt QML imports, Wayland plugins, the KDE
platform theme, or the Breeze widget style because those can pull Plasma,
WebEngine, multimedia, and other large dependency trees into a development
AppImage. Enable optional pieces explicitly when needed:

```sh
BUNDLE_QT_TRANSLATIONS=1 \
BUNDLE_QML=1 \
BUNDLE_WAYLAND=1 \
BUNDLE_PLATFORMTHEMES=1 \
BUNDLE_BREEZE_STYLE=1 \
linux-build/scripts/prepare-okular-appdir.sh
```

Keep debug symbols in the staged AppDir with:

```sh
STRIP_APPDIR=0 linux-build/scripts/prepare-okular-appdir.sh
```

## Build the AppImage

Install or download `appimagetool`, then run:

```sh
APPIMAGETOOL=/path/to/appimagetool \
linux-build/scripts/build-okular-appimage.sh
```

If `appimagetool` is not on `PATH`, the script can download the continuous
`appimagetool` AppImage:

```sh
DOWNLOAD_APPIMAGETOOL=1 linux-build/scripts/build-okular-appimage.sh
```

For a persistent local install of `appimagetool`, keep the AppImage under
`$HOME/.local/opt/appimagetool` and expose a wrapper on `PATH`:

```sh
mkdir -p "$HOME/.local/opt/appimagetool" "$HOME/.local/bin"
curl -L --fail \
  -o "$HOME/.local/opt/appimagetool/appimagetool-x86_64.AppImage" \
  https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
chmod 755 "$HOME/.local/opt/appimagetool/appimagetool-x86_64.AppImage"

curl -L --fail \
  -o "$HOME/.local/opt/appimagetool/runtime-x86_64" \
  https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
chmod 755 "$HOME/.local/opt/appimagetool/runtime-x86_64"

cat > "$HOME/.local/bin/appimagetool" <<'EOF'
#!/bin/sh
tool="${APPIMAGETOOL_REAL:-$HOME/.local/opt/appimagetool/appimagetool-x86_64.AppImage}"
if [ -z "${APPIMAGE_EXTRACT_AND_RUN+x}" ] && [ ! -e /dev/fuse ]; then
    export APPIMAGE_EXTRACT_AND_RUN=1
fi
exec "$tool" "$@"
EOF
chmod 755 "$HOME/.local/bin/appimagetool"
```

When `$HOME/.local/opt/appimagetool/runtime-x86_64` exists, the build script
passes it to `appimagetool` with `--runtime-file` so AppImage generation does
not need to download the runtime every time.

The output defaults to:

```sh
build-appimage/Okular-dev-<git-sha>-x86_64.AppImage
```

The build script passes `-n` to `appimagetool` by default. This skips
AppStream URL validation, which is not useful for a local development AppImage
and can fail in offline or proxied environments. Override it with:

```sh
APPIMAGETOOL_FLAGS= linux-build/scripts/build-okular-appimage.sh
```

## Runtime behavior

The AppImage `AppRun` wrapper sets:

```sh
LD_LIBRARY_PATH=$APPDIR/usr/lib64
XDG_DATA_DIRS=$APPDIR/usr/share:...
QT_PLUGIN_PATH=$APPDIR/usr/lib64/plugins
QT_QPA_PLATFORM_PLUGIN_PATH=$APPDIR/usr/lib64/plugins/platforms
QML2_IMPORT_PATH=$APPDIR/usr/lib64/qml
QT_TRANSLATION_PATH=$APPDIR/usr/share/qt6/translations
POPPLER_DATADIR=$APPDIR/usr/share/poppler
```

`POPPLER_DATADIR` is important. It prevents the AppImage from depending on the
host system's `/usr/share/poppler`, which is the usual source of broken CJK PDF
rendering on machines without `poppler-data`.

## Verify

Check the AppDir before building the AppImage:

```sh
test -d build-appimage/Okular.AppDir/usr/share/poppler/cMap/Adobe-GB1
test -f build-appimage/Okular.AppDir/usr/lib64/plugins/okular_generators/okularGenerator_poppler.so
test -f build-appimage/Okular.AppDir/usr/lib64/plugins/platforms/libqxcb.so
```

Inspect unresolved dependencies:

```sh
LD_LIBRARY_PATH=$PWD/build-appimage/Okular.AppDir/usr/lib64 \
ldd build-appimage/Okular.AppDir/usr/bin/okular | rg 'not found|\.local/opt/okular'
```

The preparation script performs the same kind of check for all staged ELF files
and fails if anything still resolves through the local build prefix.

Run the AppDir directly:

```sh
build-appimage/Okular.AppDir/AppRun /path/to/test.pdf
```

Run the final AppImage:

```sh
./build-appimage/Okular-dev-*-x86_64.AppImage /path/to/test.pdf
```

If the host lacks FUSE support, run with:

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./build-appimage/Okular-dev-*-x86_64.AppImage
```

## Limits

This is a practical development AppImage, not yet a distro-neutral release
artifact. It is built from the current host system, so the result is most
reliable on systems with a compatible or newer glibc and graphics stack.
OpenGL, EGL, Vulkan, and DRM libraries are intentionally not bundled; those
should come from the target system's graphics driver stack.
