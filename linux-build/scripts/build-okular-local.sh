#!/bin/sh
set -eu

usage()
{
    cat <<'EOF'
Usage: linux-build/scripts/build-okular-local.sh [options]

Build the local Poppler submodule and Okular using the workflow documented in
README.local-linux-build.md.

Options:
  --configure-only       Configure Poppler and Okular, but do not build or install.
  --no-install           Do not run any cmake --install step.
  --no-okular-install    Install Poppler into the prefix, but do not install Okular.
  --skip-poppler         Do not configure, build, or install Poppler.
  --jobs N               Parallel build jobs. Default: 8.
  --prefix PATH          Install prefix. Default: $HOME/.local/opt/okular.
  --build-root PATH      Build root. Default: <repo>/../linux_build.
  -h, --help             Show this help.

The default paths intentionally mirror the README:
  Poppler build: <repo>/../linux_build/poppler-local
  Okular build:  <repo>/../linux_build/okular-local-poppler
EOF
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
workspace_dir=$(CDPATH= cd -- "$repo_dir/.." && pwd)

prefix="${PREFIX:-$HOME/.local/opt/okular}"
build_root="${BUILD_ROOT:-$workspace_dir/linux_build}"
jobs="${JOBS:-8}"
configure_only=0
install_poppler=1
install_okular=1
skip_poppler=0

while [ $# -gt 0 ]; do
    case "$1" in
        --configure-only)
            configure_only=1
            install_poppler=0
            install_okular=0
            ;;
        --no-install)
            install_poppler=0
            install_okular=0
            ;;
        --no-okular-install)
            install_okular=0
            ;;
        --skip-poppler)
            skip_poppler=1
            ;;
        --jobs)
            shift
            if [ $# -eq 0 ]; then
                echo "--jobs requires a value" >&2
                exit 2
            fi
            jobs=$1
            ;;
        --prefix)
            shift
            if [ $# -eq 0 ]; then
                echo "--prefix requires a value" >&2
                exit 2
            fi
            prefix=$1
            ;;
        --build-root)
            shift
            if [ $# -eq 0 ]; then
                echo "--build-root requires a value" >&2
                exit 2
            fi
            build_root=$1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

poppler_build="$build_root/poppler-local"
okular_build="$build_root/okular-local-poppler"

cd "$repo_dir"

echo "Local Linux build"
echo "  Repo:             $repo_dir"
echo "  Build root:       $build_root"
echo "  Prefix:           $prefix"
echo "  Jobs:             $jobs"
echo "  Configure only:   $configure_only"
echo "  Skip Poppler:     $skip_poppler"
echo "  Install Poppler:  $install_poppler"
echo "  Install Okular:   $install_okular"

git submodule update --init --recursive

"$script_dir/install-poppler-data.sh" "$prefix"

if [ "$skip_poppler" != "1" ]; then
    cmake -S external/poppler -B "$poppler_build" \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DPOPPLER_DATADIR="$prefix/share/poppler" \
        -DENABLE_QT6=ON \
        -DENABLE_QT5=OFF

    if [ "$configure_only" != "1" ]; then
        cmake --build "$poppler_build" -j "$jobs"
        if [ "$install_poppler" = "1" ]; then
            cmake --install "$poppler_build"
        fi
    fi
fi

PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$prefix/lib64/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
cmake -S . -B "$okular_build" \
    -DCMAKE_PREFIX_PATH="$prefix" \
    -DOKULAR_ENABLE_MICROTEX=ON \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DKDE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_PREFIX="$prefix"

if [ "$configure_only" != "1" ]; then
    cmake --build "$okular_build" -j "$jobs"
    if [ "$install_okular" = "1" ]; then
        cmake --install "$okular_build"
    fi
fi
