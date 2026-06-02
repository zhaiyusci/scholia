#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)

prefix="${PREFIX:-$HOME/.local/opt/okular}"
appdir="${APPDIR:-$repo_dir/build-appimage/Okular.AppDir}"
pdf_only="${PDF_ONLY:-1}"
bundle_qml="${BUNDLE_QML:-0}"
bundle_breeze_icons="${BUNDLE_BREEZE_ICONS:-1}"
bundle_wayland="${BUNDLE_WAYLAND:-0}"
bundle_platformthemes="${BUNDLE_PLATFORMTHEMES:-0}"
bundle_breeze_style="${BUNDLE_BREEZE_STYLE:-0}"
bundle_qt_translations="${BUNDLE_QT_TRANSLATIONS:-0}"
strip_appdir="${STRIP_APPDIR:-1}"

if [ ! -x "$prefix/bin/okular" ]; then
    echo "Okular binary not found in prefix: $prefix/bin/okular" >&2
    echo "Build and install the local Linux build first; see README.local-linux-build.md." >&2
    exit 1
fi

if [ ! -d "$prefix/share/poppler/cMap/Adobe-GB1" ]; then
    "$script_dir/install-poppler-data.sh" "$prefix"
fi

prefix_libdir="$prefix/lib64"
if [ ! -d "$prefix_libdir" ]; then
    prefix_libdir="$prefix/lib"
fi
libdir_name=$(basename "$prefix_libdir")
app_usr="$appdir/usr"
app_libdir="$app_usr/$libdir_name"
work_dir="$repo_dir/build-appimage"

qtpaths_bin="${QTPATHS:-}"
if [ -z "$qtpaths_bin" ]; then
    qtpaths_bin=$(command -v qtpaths6 || command -v qtpaths || true)
fi

qt_query()
{
    if [ -n "$qtpaths_bin" ]; then
        "$qtpaths_bin" --qt-query "$1" 2>/dev/null | sed 's/^[^:]*://'
    fi
}

copy_dir()
{
    src=$1
    dst=$2
    label=$3
    if [ -d "$src" ]; then
        mkdir -p "$dst"
        rsync -a --no-owner --no-group --delete "$src"/ "$dst"/
        echo "Copied $label"
    fi
}

copy_file()
{
    src=$1
    dst=$2
    label=$3
    if [ -f "$src" ]; then
        mkdir -p "$(dirname -- "$dst")"
        cp -aL "$src" "$dst"
        chmod u+w "$dst" 2>/dev/null || true
        echo "Copied $label"
    fi
}

copy_required_file()
{
    src=$1
    dst=$2
    label=$3
    if [ ! -f "$src" ]; then
        echo "Missing required file for $label: $src" >&2
        exit 1
    fi
    copy_file "$src" "$dst" "$label"
}

copy_prefix_file()
{
    rel=$1
    label=$2
    copy_required_file "$prefix/$rel" "$app_usr/$rel" "$label"
}

copy_prefix_dir()
{
    rel=$1
    label=$2
    copy_dir "$prefix/$rel" "$app_usr/$rel" "$label"
}

copy_optional_prefix_file()
{
    rel=$1
    label=$2
    copy_file "$prefix/$rel" "$app_usr/$rel" "$label"
}

copy_soname_library()
{
    src=$1
    label=$2
    if [ ! -f "$src" ]; then
        echo "Missing required library for $label: $src" >&2
        exit 1
    fi
    soname=$(readelf -d "$src" 2>/dev/null | awk -F'[][]' '/SONAME/ { print $2; exit }')
    if [ -z "$soname" ]; then
        soname=$(basename -- "$src")
    fi
    copy_file "$src" "$app_libdir/$soname" "$label $soname"
}

copy_qt_plugin_file()
{
    rel=$1
    copy_file "$qt_plugin_dir/$rel" "$app_libdir/plugins/$rel" "Qt plugin $rel"
}

echo "Preparing AppDir:"
echo "  Prefix: $prefix"
echo "  AppDir: $appdir"
echo "  PDF_ONLY: $pdf_only"
echo "  STRIP_APPDIR: $strip_appdir"

rm -rf "$appdir"
mkdir -p "$app_usr/bin" "$app_libdir" "$work_dir"

copy_prefix_file bin/okular "Okular executable"
copy_soname_library "$prefix_libdir/libOkular6Core.so.4" "Okular core library"

copy_prefix_file "$libdir_name/plugins/kf6/parts/okularpart.so" "Okular KParts UI plugin"
if [ "$pdf_only" != "0" ]; then
    copy_prefix_file "$libdir_name/plugins/okular_generators/okularGenerator_poppler.so" "PDF generator"
    copy_optional_prefix_file share/applications/okularApplication_pdf.desktop "PDF application metadata"
    copy_optional_prefix_file share/metainfo/org.kde.okular-poppler.metainfo.xml "PDF generator metadata"
else
    copy_prefix_dir "$libdir_name/plugins/okular_generators" "Okular generators"
    copy_prefix_dir share/applications "Okular application metadata"
    copy_prefix_dir share/metainfo "Okular metainfo"
fi

copy_prefix_dir share/okular "Okular annotation tools and app data"
copy_prefix_dir share/icons/hicolor "Okular hicolor icons"
copy_prefix_dir share/config.kcfg "Okular configuration schemas"
copy_prefix_dir share/qlogging-categories6 "Okular logging category metadata"
copy_prefix_dir share/poppler "Poppler CMap/CID data"
copy_optional_prefix_file share/applications/org.kde.okular.desktop "Okular desktop metadata"
copy_optional_prefix_file share/metainfo/org.kde.okular.appdata.xml "Okular app metadata"

qt_plugin_dir="${QT_PLUGIN_DIR:-$(qt_query QT_INSTALL_PLUGINS)}"
qt_qml_dir="${QT_QML_DIR:-$(qt_query QT_INSTALL_QML)}"
qt_translations_dir="${QT_TRANSLATIONS_DIR:-$(qt_query QT_INSTALL_TRANSLATIONS)}"

if [ -n "$qt_plugin_dir" ] && [ -d "$qt_plugin_dir" ]; then
    for rel in \
        platforms/libqxcb.so \
        platforms/libqminimal.so \
        platforms/libqoffscreen.so \
        imageformats/libqgif.so \
        imageformats/libqico.so \
        imageformats/libqjpeg.so \
        imageformats/libqsvg.so \
        iconengines/libqsvgicon.so \
        kiconthemes6/iconengines/KIconEnginePlugin.so \
        platforminputcontexts/libcomposeplatforminputcontextplugin.so \
        platforminputcontexts/libibusplatforminputcontextplugin.so \
        tls/libqcertonlybackend.so \
        tls/libqopensslbackend.so \
        networkinformation/libqglib.so \
        printsupport/libcupsprintersupport.so
    do
        copy_qt_plugin_file "$rel"
    done

    if [ "$bundle_wayland" != "0" ]; then
        copy_qt_plugin_file platforms/libqwayland.so
        for rel in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
            copy_dir "$qt_plugin_dir/$rel" "$app_libdir/plugins/$rel" "Qt plugin directory $rel"
        done
    fi

    if [ "$bundle_platformthemes" != "0" ]; then
        copy_qt_plugin_file platformthemes/KDEPlasmaPlatformTheme6.so
        copy_qt_plugin_file platformthemes/libqgtk3.so
    fi

    if [ "$bundle_breeze_style" != "0" ]; then
        copy_qt_plugin_file styles/breeze6.so
    fi

    for rel in \
        kf6/kwindowsystem \
        kf6/kauth \
        kf6/urifilters
    do
        copy_dir "$qt_plugin_dir/$rel" "$app_libdir/plugins/$rel" "KF6 plugin directory $rel"
    done

    for rel in \
        kf6/kio/kio_file.so \
        kf6/kio/kio_http.so \
        kf6/kio/kio_trash.so \
        kf6/kio/filter.so
    do
        copy_file "$qt_plugin_dir/$rel" "$app_libdir/plugins/$rel" "KF6 KIO plugin $rel"
    done
fi

if [ "$bundle_qml" != "0" ] && [ -n "$qt_qml_dir" ] && [ -d "$qt_qml_dir" ]; then
    copy_dir "$qt_qml_dir" "$app_libdir/qml" "Qt QML imports"
fi

if [ "$bundle_qt_translations" != "0" ] && [ -n "$qt_translations_dir" ] && [ -d "$qt_translations_dir" ]; then
    mkdir -p "$app_usr/share/qt6/translations"
    rsync -a --no-owner --no-group --delete \
        --exclude=/qtwebengine_locales/ \
        "$qt_translations_dir"/ "$app_usr/share/qt6/translations"/
    echo "Copied Qt translations"
fi

for rel in kf6 knotifications6 mime; do
    copy_dir "/usr/share/$rel" "$app_usr/share/$rel" "system runtime data $rel"
done

if [ "$bundle_breeze_icons" != "0" ]; then
    copy_dir /usr/share/icons/breeze "$app_usr/share/icons/breeze" "Breeze icons"
fi

copy_file /usr/libexec/kf6/kioworker "$app_usr/libexec/kf6/kioworker" "KIO worker"
copy_file /usr/libexec/kf6/kioworker "$app_libdir/libexec/kf6/kioworker" "KIO worker libdir fallback"

cat > "$app_usr/bin/qt.conf" <<EOF
[Paths]
Prefix=..
Plugins=$libdir_name/plugins
Qml2Imports=$libdir_name/qml
Translations=share/qt6/translations
EOF

desktop_src="$app_usr/share/applications/org.kde.okular.desktop"
desktop_dst="$appdir/org.kde.okular.desktop"
if [ ! -f "$desktop_src" ]; then
    echo "Missing desktop file: $desktop_src" >&2
    exit 1
fi
awk '
    /^Exec=/ { print "Exec=okular %U"; next }
    /^Icon=/ { print "Icon=okular"; next }
    /^Terminal=/ { print "Terminal=false"; next }
    { print }
' "$desktop_src" > "$desktop_dst"

icon_src="$app_usr/share/icons/hicolor/128x128/apps/okular.png"
if [ ! -f "$icon_src" ]; then
    echo "Missing icon file: $icon_src" >&2
    exit 1
fi
cp -a "$icon_src" "$appdir/okular.png"
cp -a "$icon_src" "$appdir/.DirIcon"

cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
this=$0
case "$this" in
    /*) ;;
    *) this=$PWD/$this ;;
esac
appdir=$(CDPATH= cd -- "$(dirname -- "$this")" && pwd)
prefix="$appdir/usr"
libdir="$prefix/lib64"
if [ ! -d "$libdir" ]; then
    libdir="$prefix/lib"
fi

export PATH="$prefix/bin${PATH:+:$PATH}"
export LD_LIBRARY_PATH="$libdir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="$prefix/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export XDG_CONFIG_DIRS="$prefix/etc/xdg:${XDG_CONFIG_DIRS:-/etc/xdg}"
export QT_PLUGIN_PATH="$libdir/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$libdir/plugins/platforms"
export QML2_IMPORT_PATH="$libdir/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
export QT_QUICK_CONTROLS_STYLE_PATH="$libdir/qml/QtQuick/Controls.2${QT_QUICK_CONTROLS_STYLE_PATH:+:$QT_QUICK_CONTROLS_STYLE_PATH}"
export QT_TRANSLATION_PATH="$prefix/share/qt6/translations${QT_TRANSLATION_PATH:+:$QT_TRANSLATION_PATH}"
export SASL_PATH="$libdir/sasl2${SASL_PATH:+:$SASL_PATH}"
export POPPLER_DATADIR="$prefix/share/poppler"
okular_default_logging_rules="kf.kio.workers.file.debug=false;kf.kio.workers.file.info=false"
export QT_LOGGING_RULES="$okular_default_logging_rules${QT_LOGGING_RULES:+;$QT_LOGGING_RULES}"

if [ -d "$prefix/share/icons/breeze" ]; then
    export QT_ICON_THEME="${QT_ICON_THEME:-breeze}"
fi

exec "$prefix/bin/okular" "$@"
EOF
chmod 755 "$appdir/AppRun"

is_elf()
{
    file -Lb "$1" 2>/dev/null | grep -Eq 'ELF .* (executable|shared object|pie executable)'
}

elf_soname()
{
    readelf -d "$1" 2>/dev/null | awk -F'[][]' '/SONAME/ { print $2; exit }'
}

skip_library()
{
    base=$1
    case "$base" in
        linux-vdso*|ld-linux*|libc.so.*|libpthread.so.*|libdl.so.*|librt.so.*|libm.so.*|libutil.so.*|libresolv.so.*|libnss_*.so*|libanl.so.*|libBrokenLocale.so.*)
            return 0
            ;;
        libGL.so.*|libEGL.so.*|libGLX.so.*|libOpenGL.so.*|libGLESv*.so.*|libdrm.so.*|libgbm.so.*|libvulkan.so.*)
            return 0
            ;;
    esac
    return 1
}

collect_deps()
{
    binary=$1
    env LD_LIBRARY_PATH="$app_libdir:$prefix_libdir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ldd "$binary" 2>/dev/null |
        awk '
            /=> \// {
                for (i = 1; i <= NF; ++i) {
                    if ($i == "=>") {
                        print $(i + 1)
                        next
                    }
                }
            }
            /^\// { print $1 }
        '
}

copied=1
pass=0
while [ "$copied" -gt 0 ]; do
    pass=$((pass + 1))
    deps_file="$work_dir/deps.$pass"
    copied_file="$work_dir/copied.$pass"
    : > "$deps_file"
    : > "$copied_file"

    find "$app_usr/bin" "$app_libdir" "$app_usr/libexec" -type f 2>/dev/null |
        while IFS= read -r candidate; do
            if is_elf "$candidate"; then
                collect_deps "$candidate" >> "$deps_file"
            fi
        done

    sort -u "$deps_file" |
        while IFS= read -r dep; do
            [ -f "$dep" ] || continue
            base=$(basename -- "$dep")
            if skip_library "$base"; then
                continue
            fi
            dest_name=$(elf_soname "$dep")
            if [ -z "$dest_name" ]; then
                dest_name=$base
            fi
            if [ -e "$app_libdir/$dest_name" ]; then
                continue
            fi
            cp -aL "$dep" "$app_libdir/$dest_name"
            chmod u+w "$app_libdir/$dest_name" 2>/dev/null || true
            echo "$dest_name" >> "$copied_file"
            echo "Copied dependency $dest_name"
        done

    copied=$(wc -l < "$copied_file" | tr -d ' ')
done

if [ "$strip_appdir" != "0" ] && command -v strip >/dev/null 2>&1; then
    find "$app_usr/bin" "$app_libdir" "$app_usr/libexec" -type f 2>/dev/null |
        while IFS= read -r candidate; do
            if is_elf "$candidate"; then
                strip --strip-unneeded "$candidate" 2>/dev/null || true
            fi
        done
    echo "Stripped AppDir ELF binaries"
fi

final_deps_file="$work_dir/final-deps.txt"
: > "$final_deps_file"
find "$app_usr/bin" "$app_libdir" "$app_usr/libexec" -type f 2>/dev/null |
    while IFS= read -r candidate; do
        if is_elf "$candidate"; then
            env LD_LIBRARY_PATH="$app_libdir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ldd "$candidate" 2>/dev/null >> "$final_deps_file" || true
        fi
    done

if grep -q 'not found' "$final_deps_file"; then
    echo "Unresolved dependencies remain:" >&2
    grep 'not found' "$final_deps_file" >&2
    exit 1
fi

if grep -F -q "$prefix/" "$final_deps_file"; then
    echo "Staged AppDir still depends on the build prefix:" >&2
    grep -F "$prefix/" "$final_deps_file" >&2
    exit 1
fi

find "$appdir" -type f \( -name '*.desktop' -o -name '*.xml' -o -name '*.appdata.xml' -o -name '*.metainfo.xml' \) \
    -exec sed -i 's/\r$//' {} +

for required in \
    "$app_usr/bin/okular" \
    "$app_usr/share/poppler/cMap/Adobe-GB1" \
    "$app_usr/share/poppler/cidToUnicode" \
    "$app_libdir/plugins/okular_generators/okularGenerator_poppler.so" \
    "$app_libdir/plugins/platforms/libqxcb.so" \
    "$appdir/AppRun" \
    "$appdir/org.kde.okular.desktop" \
    "$appdir/okular.png"
do
    if [ ! -e "$required" ]; then
        echo "Required AppDir component missing: $required" >&2
        exit 1
    fi
done

du -sh "$appdir"
echo "Prepared AppDir at $appdir"
