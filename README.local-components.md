# Local component submodules

This fork pins two non-upstream components as git submodules:

- `external/poppler`: the Poppler fork/branch used for vector stamp appearance and Okular LaTeX note metadata.
- `external/MicroTeX`: the MicroTeX fallback renderer used when a system TeX executable is unavailable.

Initialize them after cloning:

```sh
git submodule update --init --recursive
```

MicroTeX is consumed directly by Okular when configuring with:

```sh
-DOKULAR_ENABLE_MICROTEX=ON
```

If `MICROTEX_SRC` is not set, Okular first looks at `external/MicroTeX`.

Poppler is different: Okular still consumes Poppler through `find_package(Poppler)`.
Build and install the Poppler submodule to a local prefix first, then configure Okular with that prefix in `CMAKE_PREFIX_PATH`.
The local Poppler build should also use bundled poppler-data from
`$HOME/.local/opt/okular/share/poppler`, not the system `/usr/share/poppler`.

The detailed Linux local build and install workflow is documented in
`README.local-linux-build.md`.

Example layout used for local development:

```sh
PREFIX=$HOME/.local/opt/okular

linux-build/scripts/install-poppler-data.sh $PREFIX

cmake -S external/poppler -B build-poppler-local \
  -DCMAKE_INSTALL_PREFIX=$PREFIX \
  -DPOPPLER_DATADIR=$PREFIX/share/poppler \
  -DENABLE_QT6=ON \
  -DENABLE_QT5=OFF
cmake --build build-poppler-local
cmake --install build-poppler-local

PKG_CONFIG_PATH=$PREFIX/lib64/pkgconfig:$PREFIX/lib/pkgconfig \
cmake -S . -B build-local-poppler \
  -DCMAKE_PREFIX_PATH=$PREFIX \
  -DOKULAR_ENABLE_MICROTEX=ON \
  -DCMAKE_INSTALL_PREFIX=$PREFIX
cmake --build build-local-poppler
cmake --install build-local-poppler
```

The user-facing launcher is `$HOME/.local/bin/okular`. It is a self-contained
wrapper that exports the local prefix environment before executing
`$HOME/.local/opt/okular/bin/okular`, so the local plugins, data files, and
Poppler libraries are found without replacing the system Okular package. It
must not source files from the source or build tree. It should export
`POPPLER_DATADIR=$HOME/.local/opt/okular/share/poppler` so the local Poppler
does not depend on system CMap data.
