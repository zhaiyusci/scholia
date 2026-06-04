#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
workspace_dir=$(CDPATH= cd -- "$repo_dir/.." && pwd)
appimage_build_dir="${APPIMAGE_BUILD_DIR:-$workspace_dir/linux_build/appimage}"

appdir="${APPDIR:-$appimage_build_dir/Okular.AppDir}"
output_dir="${APPIMAGE_OUTPUT_DIR:-$appimage_build_dir}"
tool_dir="${APPIMAGE_TOOL_DIR:-$appimage_build_dir/tools}"
version="${VERSION:-$(date +%Y%m%d)}"
arch="${ARCH:-x86_64}"
output="${APPIMAGE_OUTPUT:-$output_dir/Okular-dev-$version-$arch.AppImage}"
appimagetool_flags="${APPIMAGETOOL_FLAGS:--n}"
default_runtime_file="$HOME/.local/opt/appimagetool/runtime-$arch"
if [ -f "$tool_dir/runtime-$arch" ]; then
    default_runtime_file="$tool_dir/runtime-$arch"
fi
runtime_file="${APPIMAGE_RUNTIME_FILE:-$default_runtime_file}"

download_file()
{
    url=$1
    dst=$2
    mkdir -p "$(dirname -- "$dst")"
    if command -v curl >/dev/null 2>&1; then
        curl -L --fail "$url" -o "$dst"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$dst" "$url"
    else
        echo "Neither curl nor wget is available to download $url." >&2
        exit 1
    fi
    chmod 755 "$dst"
}

normalize_appdir_metadata()
{
    find "$appdir" -type f \( -name '*.desktop' -o -name '*.xml' -o -name '*.appdata.xml' -o -name '*.metainfo.xml' \) \
        -exec sed -i 's/\r$//' {} +
}

if [ "${SKIP_PREPARE_APPDIR:-0}" != "1" ]; then
    "$script_dir/prepare-okular-appdir.sh"
else
    if [ ! -x "$appdir/AppRun" ]; then
        echo "Existing AppDir is missing or incomplete: $appdir" >&2
        echo "Rerun without SKIP_PREPARE_APPDIR=1 first." >&2
        exit 1
    fi
fi

if [ "${NORMALIZE_APPDIR_METADATA:-1}" != "0" ]; then
    normalize_appdir_metadata
fi

appimagetool="${APPIMAGETOOL:-}"
if [ -z "$appimagetool" ]; then
    appimagetool=$(command -v appimagetool || true)
fi

if [ -z "$appimagetool" ]; then
    mkdir -p "$tool_dir"
    appimagetool="$tool_dir/appimagetool-$arch.AppImage"
    if [ ! -x "$appimagetool" ]; then
        if [ "${DOWNLOAD_APPIMAGETOOL:-0}" != "1" ]; then
            echo "appimagetool was not found." >&2
            echo "Set APPIMAGETOOL=/path/to/appimagetool, or rerun with DOWNLOAD_APPIMAGETOOL=1." >&2
            exit 1
        fi
        url="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$arch.AppImage"
        download_file "$url" "$appimagetool"
    fi
fi

if [ ! -f "$runtime_file" ] && [ "${DOWNLOAD_APPIMAGE_RUNTIME:-0}" = "1" ]; then
    url="https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-$arch"
    download_file "$url" "$runtime_file"
fi

mkdir -p "$output_dir"
rm -f "$output"

export ARCH="$arch"
export VERSION="$version"
export APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}"

if [ -f "$runtime_file" ]; then
    appimagetool_flags="$appimagetool_flags --runtime-file $runtime_file"
fi

"$appimagetool" $appimagetool_flags "$appdir" "$output"
chmod 755 "$output"
echo "Created AppImage: $output"
