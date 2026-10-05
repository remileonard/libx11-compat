#!/bin/sh
# Populate an Open Inventor font directory from the host's fonts.
#
# Open Inventor's font library (libFL, over FreeType) opens a font by joining
# its font path (FL_FONT_PATH, default the install-time IVFONTPATH) with the
# PostScript-style name the scene asks for ("Times-Roman", "Utopia-Regular",
# the "defaultFont" placeholder...). Nothing installs files under those names, so
# SoText3/SoText2 render nothing. This links each name to the closest host font
# fontconfig finds; scripts/run-open-inventor.sh points FL_FONT_PATH here.
#
# Usage: open-inventor-fonts.sh <dir>
set -eu

dir=${1:?usage: open-inventor-fonts.sh <dir>}
mkdir -p "$dir"

command -v fc-match >/dev/null 2>&1 || {
    echo "open-inventor-fonts: no fc-match; 3D text will not render" >&2
    exit 0
}

# name|fontconfig pattern. Utopia (SoFontStyle's SERIF family and the fallback
# for any missing font) and the "defaultFont" placeholder map to the generic
# serif family; the rest keep their family name and let fontconfig substitute.
while IFS='|' read -r name pattern; do
    [ -n "$name" ] || continue
    file=$(fc-match -f '%{file}' "$pattern" 2>/dev/null || true)
    if [ -n "$file" ] && [ -r "$file" ]; then
        ln -sf "$file" "$dir/$name"
    else
        echo "open-inventor-fonts: no host font for $name ($pattern)" >&2
    fi
done <<'EOF'
defaultFont|serif
Utopia-Regular|serif
Utopia-Bold|serif:bold
Utopia-Italic|serif:italic
Utopia-BoldItalic|serif:bold:italic
Times-Roman|Times
Times-Bold|Times:bold
Times-Italic|Times:italic
Times-BoldItalic|Times:bold:italic
Helvetica|Helvetica
Helvetica-Bold|Helvetica:bold
Helvetica-Oblique|Helvetica:italic
Helvetica-BoldOblique|Helvetica:bold:italic
Courier|Courier
Courier-Bold|Courier:bold
Courier-Oblique|Courier:italic
Courier-BoldOblique|Courier:bold:italic
Palatino-Roman|Palatino
Palatino-Bold|Palatino:bold
Palatino-Italic|Palatino:italic
Palatino-BoldItalic|Palatino:bold:italic
EOF
