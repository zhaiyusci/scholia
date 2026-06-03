# Local Linux build

This fork is developed as an isolated Linux install instead of replacing the
distribution Okular package. The local Poppler submodule and Okular are both
installed into one prefix:

```sh
PREFIX=$HOME/.local/opt/okular
```

The user-facing command is:

```sh
$HOME/.local/bin/okular
```

That command should be a small wrapper, not a plain symlink, because Okular
needs the local prefix exported through `XDG_DATA_DIRS`, `QT_PLUGIN_PATH`, and
related environment variables. The wrapper must not source files from the
source or build tree; an installed command should only depend on the install
prefix.

## Quick Command Map

Run these commands inside WSL/Linux from the repository root. If the checkout
is stored on a Windows drive, use the corresponding `/mnt/<drive>/...` path, for
example:

```sh
cd /mnt/c/path/to/okular
```

From PowerShell, enter the WSL distro with:

```powershell
wsl.exe -d openSUSE-Tumbleweed
```

Full local Linux build and install:

```sh
export PREFIX="$HOME/.local/opt/okular"
git submodule update --init --recursive

linux-build/scripts/install-poppler-data.sh "$PREFIX"

cmake -S external/poppler -B build-poppler-local \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DPOPPLER_DATADIR="$PREFIX/share/poppler" \
  -DENABLE_QT6=ON \
  -DENABLE_QT5=OFF
cmake --build build-poppler-local -j 8
cmake --install build-poppler-local

PKG_CONFIG_PATH="$PREFIX/lib64/pkgconfig:$PREFIX/lib/pkgconfig" \
cmake -S . -B build-local-poppler \
  -DCMAKE_PREFIX_PATH="$PREFIX" \
  -DOKULAR_ENABLE_MICROTEX=ON \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build-local-poppler -j 8
cmake --install build-local-poppler
```

Create or refresh the launcher after installing:

```sh
mkdir -p "$HOME/.local/bin"
cat > "$HOME/.local/bin/okular" <<'EOF'
#!/bin/sh
prefix="${OKULAR_LOCAL_PREFIX:-$HOME/.local/opt/okular}"
libdir="$prefix/lib64"
if [ ! -d "$libdir" ]; then
    libdir="$prefix/lib"
fi

export PATH="$prefix/bin${PATH:+:$PATH}"
export XDG_DATA_DIRS="$prefix/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export XDG_CONFIG_DIRS="$prefix/etc/xdg:${XDG_CONFIG_DIRS:-/etc/xdg}"
export QT_PLUGIN_PATH="$libdir/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QML2_IMPORT_PATH="$libdir/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
export QT_QUICK_CONTROLS_STYLE_PATH="$libdir/qml/QtQuick/Controls.2${QT_QUICK_CONTROLS_STYLE_PATH:+:$QT_QUICK_CONTROLS_STYLE_PATH}"
export MANPATH="$prefix/share/man:${MANPATH:-/usr/local/share/man:/usr/share/man}"
export SASL_PATH="$libdir/sasl2:${SASL_PATH:-/usr/lib64/sasl2}"
export POPPLER_DATADIR="$prefix/share/poppler"
okular_default_logging_rules="kf.kio.workers.file.debug=false;kf.kio.workers.file.info=false"
export QT_LOGGING_RULES="$okular_default_logging_rules${QT_LOGGING_RULES:+;$QT_LOGGING_RULES}"

exec "$prefix/bin/okular" "$@"
EOF
chmod 755 "$HOME/.local/bin/okular"
```

Run the installed local build:

```sh
~/.local/bin/okular /path/to/test.pdf
```

Under WSLg, prefer XCB if a Qt 6 Wayland window appears but does not show up or
does not become interactive on the Windows desktop:

```sh
QT_QPA_PLATFORM=xcb ~/.local/bin/okular /path/to/test.pdf
```

After pulling normal source changes, an incremental rebuild is usually enough:

```sh
export PREFIX="$HOME/.local/opt/okular"
git submodule update --init --recursive
linux-build/scripts/install-poppler-data.sh "$PREFIX"
cmake --build build-poppler-local -j 8
cmake --install build-poppler-local
cmake --build build-local-poppler -j 8
cmake --install build-local-poppler
```

Reconfigure from scratch if the Poppler submodule, CMake options, Qt/KF
packages, MicroTeX source, or install prefix changed.

## Layout

- Okular source: this repository
- Poppler submodule: `external/poppler`
- MicroTeX submodule: `external/MicroTeX`
- Poppler build directory: `build-poppler-local`
- Okular build directory: `build-local-poppler`
- Install prefix: `$HOME/.local/opt/okular`
- Poppler encoding data: `$HOME/.local/opt/okular/share/poppler`
- Launcher: `$HOME/.local/bin/okular`

## Prepare submodules

After cloning or pulling this fork, initialize the local component submodules:

```sh
git submodule update --init --recursive
```

`external/poppler` is used for vector stamp appearances and LaTeX note
metadata. `external/MicroTeX` is used as the fallback LaTeX renderer when a
system TeX executable is unavailable.

## WSL and openSUSE notes

The Linux scripts are meant to run inside Linux, including openSUSE under WSL.
When the repository is checked out on Windows, keep `linux-build/scripts/*.sh`
with LF line endings. The repository `.gitattributes` pins those scripts to LF;
after adding that rule to an existing checkout, refresh the files if WSL reports
`/bin/sh^M`:

```sh
git add --renormalize linux-build/scripts
```

On openSUSE Tumbleweed, install the development packages before configuring the
local build:

```sh
sudo zypper install \
  cmake ninja gcc-c++ git pkgconf-pkg-config rsync patchelf binutils curl wget file which \
  kf6-extra-cmake-modules qt6-base-devel qt6-base-private-devel qt6-tools-devel \
  qt6-svg-devel qt6-declarative-devel qt6-texttospeech-devel qt6-multimedia-devel \
  qt6-imageformats \
  kf6-karchive-devel kf6-kbookmarks-devel kf6-kcompletion-devel kf6-kconfig-devel \
  kf6-kconfigwidgets-devel kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kio-devel \
  kf6-threadweaver-devel kf6-kwindowsystem-devel kf6-kxmlgui-devel \
  kf6-kiconthemes-devel kf6-kparts-devel kf6-kcolorscheme-devel kf6-kcrash-devel \
  kf6-ktextwidgets-devel kf6-kwidgetsaddons-devel kf6-kdoctools-devel \
  kf6-purpose-devel kf6-kdbusaddons-devel kf6-kwallet-devel \
  phonon-qt6-devel libkexiv2-qt6-devel \
  poppler-data freetype2-devel fontconfig-devel cairo-devel libtiff-devel zlib-devel \
  libspectre-devel libdjvulibre-devel libepub-devel libmarkdown-devel \
  libjpeg8-devel libpng16-devel openjpeg2-devel liblcms2-devel libcurl-devel \
  mozilla-nss-devel gpgme-devel gpgmepp-devel boost-devel tinyxml2-devel
```

If `qt6-base-private-devel` conflicts with `libressl-devel`, let zypper switch
the development SSL stack to the OpenSSL packages. The Qt private headers are
required by Okular's current CMake configuration.

WSLg exposes both Wayland and Xwayland. The local installed build can run on
Wayland, but this development environment has also shown cases where Qt Wayland
creates a window with size `0x0` or where the Windows-side RemoteApp window is
not usable. In those cases, restart WSLg and force XCB:

```powershell
wsl.exe --shutdown
```

```sh
QT_QPA_PLATFORM=xcb ~/.local/bin/okular /path/to/test.pdf
```

The Windows-side WSLg window is hosted by `msrdc.exe`, not by a native Windows
`okular.exe` process. If a WSL GUI process is running but no window is visible,
check Windows Task Manager or Alt-Tab for a title ending in
`(openSUSE-Tumbleweed)`.

## Clean local build directories

For a full local rebuild, remove the two build directories and the install
prefix:

```sh
rm -rf build-poppler-local build-local-poppler
rm -rf "$PREFIX"
```

Do not remove unrelated system packages. This layout is intended to coexist
with the system Okular and system Poppler.

## Install poppler-data into the local prefix

Poppler uses external CMap and Unicode mapping data for many CJK PDFs. Do not
leave the local build dependent on the system `/usr/share/poppler` directory;
copy `poppler-data` into the local prefix before configuring Poppler:

```sh
linux-build/scripts/install-poppler-data.sh "$PREFIX"
```

By default the script copies from `/usr/share/poppler`. If the system package
is not installed, install the distribution `poppler-data` package first, or
pass an extracted poppler-data directory explicitly:

```sh
linux-build/scripts/install-poppler-data.sh "$PREFIX" /path/to/poppler-data
```

## Build and install Poppler

Configure Poppler from the submodule and install it into the shared local
prefix:

```sh
cmake -S external/poppler -B build-poppler-local \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DPOPPLER_DATADIR="$PREFIX/share/poppler" \
  -DENABLE_QT6=ON \
  -DENABLE_QT5=OFF

cmake --build build-poppler-local -j 8
cmake --install build-poppler-local
```

This installs the Poppler core and Qt 6 wrapper under
`$PREFIX/lib64` on the current Linux setup. `POPPLER_DATADIR` must point into
the same local prefix so the installed Okular does not rely on the system
`poppler-data` package.

The Poppler build may regenerate files under `external/poppler`, such as
`poppler/*Widths.pregenerated.c` and `utils/po/pdfsig.pot`, especially when
`clang-format` is not installed. Treat those as build byproducts unless the
submodule update itself was intentional.

## Build and install Okular

Configure Okular against the Poppler installed above. `PKG_CONFIG_PATH` is set
explicitly so CMake and pkg-config prefer the local Poppler over the system
copy:

```sh
PKG_CONFIG_PATH="$PREFIX/lib64/pkgconfig:$PREFIX/lib/pkgconfig" \
cmake -S . -B build-local-poppler \
  -DCMAKE_PREFIX_PATH="$PREFIX" \
  -DOKULAR_ENABLE_MICROTEX=ON \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"

cmake --build build-local-poppler -j 8
cmake --install build-local-poppler
```

If `MICROTEX_SRC` is not set, Okular looks for MicroTeX at
`external/MicroTeX`. To use a different checkout:

```sh
cmake -S . -B build-local-poppler \
  -DMICROTEX_SRC=/path/to/MicroTeX
```

## Launcher

Create or update the user command wrapper:

```sh
mkdir -p "$HOME/.local/bin"
cat > "$HOME/.local/bin/okular" <<EOF
#!/bin/sh
prefix="\${OKULAR_LOCAL_PREFIX:-\$HOME/.local/opt/okular}"
libdir="\$prefix/lib64"
if [ ! -d "\$libdir" ]; then
    libdir="\$prefix/lib"
fi

export PATH="\$prefix/bin\${PATH:+:\$PATH}"
export XDG_DATA_DIRS="\$prefix/share:\${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export XDG_CONFIG_DIRS="\$prefix/etc/xdg:\${XDG_CONFIG_DIRS:-/etc/xdg}"
export QT_PLUGIN_PATH="\$libdir/plugins\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}"
export QML2_IMPORT_PATH="\$libdir/qml\${QML2_IMPORT_PATH:+:\$QML2_IMPORT_PATH}"
export QT_QUICK_CONTROLS_STYLE_PATH="\$libdir/qml/QtQuick/Controls.2\${QT_QUICK_CONTROLS_STYLE_PATH:+:\$QT_QUICK_CONTROLS_STYLE_PATH}"
export MANPATH="\$prefix/share/man:\${MANPATH:-/usr/local/share/man:/usr/share/man}"
export SASL_PATH="\$libdir/sasl2:\${SASL_PATH:-/usr/lib64/sasl2}"
export POPPLER_DATADIR="\$prefix/share/poppler"
okular_default_logging_rules="kf.kio.workers.file.debug=false;kf.kio.workers.file.info=false"
export QT_LOGGING_RULES="\$okular_default_logging_rules\${QT_LOGGING_RULES:+;\$QT_LOGGING_RULES}"

exec "\$prefix/bin/okular" "\$@"
EOF
chmod 755 "$HOME/.local/bin/okular"
```

This wrapper keeps the install isolated while still allowing `okular` to be
resolved from the normal user `PATH`. It intentionally does not use
`build-local-poppler/prefix.sh`, because the source tree should be removable
after installation. It also sets `POPPLER_DATADIR` so relocatable bundles such
as AppImages can point Poppler at their bundled CMap and Unicode mapping data.

## Verify

Check that the Poppler generator links against the local prefix:

```sh
ldd "$PREFIX/lib64/plugins/okular_generators/okularGenerator_poppler.so" \
  | rg 'local/opt/okular|poppler'
```

Expected local links include:

```text
libpoppler-qt6.so.3 => $HOME/.local/opt/okular/lib64/libpoppler-qt6.so.3
libpoppler.so.157 => $HOME/.local/opt/okular/lib64/libpoppler.so.157
```

Check the installed Okular plugin runpath:

```sh
readelf -d "$PREFIX/lib64/plugins/kf6/parts/okularpart.so" | rg 'RPATH|RUNPATH'
```

Expected result:

```text
Library runpath: [$HOME/.local/opt/okular/lib64]
```

Check the CMake cache if there is any doubt about which Poppler was selected:

```sh
rg 'Poppler_.*LIBRARY|Poppler_.*INCLUDE|PKG_Poppler' build-local-poppler/CMakeCache.txt
```

The libraries should resolve under `$HOME/.local/opt/okular`.

Check that Poppler was compiled to use the bundled poppler-data directory:

```sh
rg 'POPPLER_DATADIR' build-poppler-local/config.h
strings "$PREFIX/lib64/libpoppler.so.157" | rg "$PREFIX/share/poppler"
test -d "$PREFIX/share/poppler/cMap/Adobe-GB1"
```

## Rebuild after pulling

For normal development after a source pull:

```sh
git submodule update --init --recursive
linux-build/scripts/install-poppler-data.sh "$PREFIX"
cmake --build build-poppler-local -j 8
cmake --install build-poppler-local
cmake --build build-local-poppler -j 8
cmake --install build-local-poppler
```

Reconfigure from scratch if the Poppler submodule, CMake options, Qt/KF
packages, or install prefix changed.

## AppImage

After installing the local build, package it as an AppImage with the workflow
in `README.local-appimage.md`.
