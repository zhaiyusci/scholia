#!/bin/sh
set -eu

prefix="${1:-${PREFIX:-$HOME/.local/opt/okular}}"
source_dir="${2:-${POPPLER_DATA_SOURCE:-/usr/share/poppler}}"
target_dir="$prefix/share/poppler"

if [ ! -d "$source_dir" ]; then
    echo "poppler-data source directory not found: $source_dir" >&2
    echo "Install the system poppler-data package, or pass an extracted poppler-data directory as the second argument." >&2
    exit 1
fi

for required in cMap cidToUnicode nameToUnicode unicodeMap; do
    if [ ! -d "$source_dir/$required" ]; then
        echo "Invalid poppler-data source: missing $source_dir/$required" >&2
        exit 1
    fi
done

mkdir -p "$target_dir"
rsync -a --delete "$source_dir"/ "$target_dir"/

version="${POPPLER_DATA_VERSION:-}"
if [ -z "$version" ] && command -v pkg-config >/dev/null 2>&1 && pkg-config --exists poppler-data 2>/dev/null; then
    version="$(pkg-config --modversion poppler-data)"
fi
version="${version:-0.0.0}"

for pc_root in "$prefix/lib64/pkgconfig" "$prefix/lib/pkgconfig"; do
    mkdir -p "$pc_root"
    cat > "$pc_root/poppler-data.pc" <<EOF
prefix=$prefix
datadir=\${prefix}/share
poppler_datadir=\${datadir}/poppler

Name: poppler-data
Description: Poppler encoding data
Version: $version
EOF
done

echo "Installed poppler-data to $target_dir"
