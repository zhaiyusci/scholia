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
