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

Example layout used for local development:

```sh
cmake -S external/poppler -B ../poppler-build -DCMAKE_INSTALL_PREFIX=$PWD/../poppler-install
cmake --build ../poppler-build
cmake --install ../poppler-build

cmake -S . -B build-local-poppler \
  -DCMAKE_PREFIX_PATH=$PWD/../poppler-install \
  -DOKULAR_ENABLE_MICROTEX=ON \
  -DCMAKE_INSTALL_PREFIX=$PWD/../install-local-poppler
```
