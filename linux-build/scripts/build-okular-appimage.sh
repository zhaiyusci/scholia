#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)

appdir="${APPDIR:-$repo_dir/build-appimage/Okular.AppDir}"
output_dir="${APPIMAGE_OUTPUT_DIR:-$repo_dir/build-appimage}"
tool_dir="${APPIMAGE_TOOL_DIR:-$repo_dir/build-appimage/tools}"
version="${VERSION:-$(git -C "$repo_dir" rev-parse --short HEAD 2>/dev/null || date +%Y%m%d)}"
arch="${ARCH:-x86_64}"
output="${APPIMAGE_OUTPUT:-$output_dir/Okular-dev-$version-$arch.AppImage}"
appimagetool_flags="${APPIMAGETOOL_FLAGS:--n}"
runtime_file="${APPIMAGE_RUNTIME_FILE:-$HOME/.local/opt/appimagetool/runtime-$arch}"

"$script_dir/prepare-okular-appdir.sh"

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
        if command -v curl >/dev/null 2>&1; then
            curl -L "$url" -o "$appimagetool"
        elif command -v wget >/dev/null 2>&1; then
            wget -O "$appimagetool" "$url"
        else
            echo "Neither curl nor wget is available to download appimagetool." >&2
            exit 1
        fi
        chmod 755 "$appimagetool"
    fi
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
