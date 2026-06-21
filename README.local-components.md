# Local component submodules

This fork pins one non-upstream component as a git submodule:

- `external/poppler`: the Poppler fork/branch used for vector stamp appearance and Okular LaTeX note metadata.

Initialize it after cloning:

```sh
git submodule update --init --recursive
```

Poppler is consumed through `find_package(Poppler)`. Build and install the
Poppler submodule to a local prefix first, then configure Okular with that
prefix in `CMAKE_PREFIX_PATH`. The local Poppler build should also use bundled
poppler-data from `$HOME/.local/opt/okular/share/poppler`, not the system
`/usr/share/poppler`.

LaTeX note rendering uses the StemTeX renderer. StemTeX is a runtime dependency
copied into the Windows package, not a git submodule of this repository.

The detailed Linux local build and install workflow is documented in
`README.local-linux-build.md`.

Example layout used for local development:

```sh
PREFIX=$HOME/.local/opt/okular

linux-build/scripts/install-poppler-data.sh $PREFIX

cmake -S external/poppler -B ../linux_build/poppler-local \
  -DCMAKE_INSTALL_PREFIX=$PREFIX \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DPOPPLER_DATADIR=$PREFIX/share/poppler \
  -DENABLE_QT6=ON \
  -DENABLE_QT5=OFF
cmake --build ../linux_build/poppler-local
cmake --install ../linux_build/poppler-local

PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig:$PREFIX/lib64/pkgconfig \
cmake -S . -B ../linux_build/okular-local-poppler \
  -DCMAKE_PREFIX_PATH=$PREFIX \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DKDE_INSTALL_LIBDIR=lib \
  -DCMAKE_INSTALL_PREFIX=$PREFIX
cmake --build ../linux_build/okular-local-poppler
cmake --install ../linux_build/okular-local-poppler
```

The user-facing launcher is `$HOME/.local/bin/okular`. Install it from
`linux-build/scripts/okular-wrapper`; it exports the local prefix environment
before executing `$HOME/.local/opt/okular/bin/okular`, so the local plugins,
data files, and Poppler libraries are found without replacing the system Okular
package. It should export `POPPLER_DATADIR=$HOME/.local/opt/okular/share/poppler`
so the local Poppler does not depend on system CMap data.
