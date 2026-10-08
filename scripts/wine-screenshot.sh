#!/bin/sh
# Run a Windows program (make WINDOWS=1) under Wine on a private Xvfb display
# and capture that display after a delay: the real on-screen result, as a user
# would see it, rather than an in-process snapshot of libx11-compat's own
# backing store (which misses presentation bugs).
#
# The program runs from <dir> (its DLLs next to it) through
# scripts/wine-run.sh, so WINE_RUN_ARCH=win32 selects 32-bit Wine. Needs
# Xvfb and ImageMagick's import.
#
# Usage: wine-screenshot.sh <dir> <program.exe> <out.bmp|out.png> [seconds
#                           [program arguments...]]
set -eu

dir=${1:?usage: wine-screenshot.sh <dir> <program.exe> <out.png> [seconds]}
prog=${2:?usage: wine-screenshot.sh <dir> <program.exe> <out.png> [seconds]}
out=${3:?usage: wine-screenshot.sh <dir> <program.exe> <out.png> [seconds]}
seconds=${4:-15}
shift 3
[ $# -eq 0 ] || shift

for tool in Xvfb import; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "wine-screenshot: $tool not found" >&2
        exit 1
    }
done
[ -f "$dir/$prog" ] || {
    echo "wine-screenshot: missing $dir/$prog" >&2
    exit 1
}

root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
out_abs=$(cd "$(dirname "$out")" && pwd)/$(basename "$out")
log=${WINE_SCREENSHOT_LOG:-$out_abs.log}

# A display number nobody uses: the first free one from 90 up.
display=90
while [ -e "/tmp/.X$display-lock" ] || [ -e "/tmp/.X11-unix/X$display" ]; do
    display=$((display + 1))
done
Xvfb ":$display" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
xvfb=$!
cleanup() {
    kill "$xvfb" 2>/dev/null || true
    wait "$xvfb" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
i=0
while [ ! -e "/tmp/.X11-unix/X$display" ] && [ $i -lt 50 ]; do
    sleep 0.1
    i=$((i + 1))
done

rm -f "$out_abs"
(cd "$dir" && DISPLAY=":$display" timeout $((seconds + 30)) \
    "$root/scripts/wine-run.sh" "./$prog" "$@") >"$log" 2>&1 &
app=$!
sleep "$seconds"
# A .bmp is written as an uncompressed 24-bit BMP3, the format
# scripts/assert-image-content.py reads; other suffixes as ImageMagick infers.
case "$out_abs" in
    *.bmp) target="BMP3:$out_abs" ;;
    *) target=$out_abs ;;
esac
DISPLAY=":$display" import -window root -type TrueColor "$target"
# Stop the program and its Wine session (wineserver of this prefix).
kill "$app" 2>/dev/null || true
if [ "${WINE_RUN_ARCH:-win64}" = win32 ]; then
    prefix=${WINEPREFIX:-$root/build/wineprefix32}
else
    prefix=${WINEPREFIX:-$root/build/wineprefix}
fi
WINEPREFIX=$prefix wineserver -k 2>/dev/null || true
wait "$app" 2>/dev/null || true
[ -f "$out_abs" ] || {
    echo "wine-screenshot: no capture; see $log" >&2
    exit 1
}
echo "screenshot: $out"
