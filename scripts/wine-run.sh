#!/bin/sh
# Run a program from a Windows (make WINDOWS=1) build under Wine, as a
# command-line tool: stdin, stdout and the exit status pass through. It is the
# CMAKE_CROSSCOMPILING_EMULATOR of the Windows Open Inventor build, which runs
# its own build-time tools (ppp).
#
# WINE_RUN_DLL_PATH is a colon-separated list of Unix directories holding the
# DLLs the program loads; it becomes Wine's WINEPATH. WINE_RUN_ARCH=win32 runs
# a 32-bit (make WINDOWS=1 WINDOWS_ARCH=i686) program: it needs Wine's 32-bit
# loader and a win32 prefix, build/wineprefix32, created on first use.
# Otherwise the prefix is build/wineprefix. WINEPREFIX overrides either, and
# WINEDEBUG defaults to -all so Wine's own diagnostics stay out of the
# program's output.
#
# Usage: wine-run.sh <program.exe> [args...]
set -eu

[ $# -ge 1 ] || {
    echo "usage: wine-run.sh <program.exe> [args...]" >&2
    exit 2
}

arch=${WINE_RUN_ARCH:-win64}
case "$arch" in
    win32) candidates="/usr/lib/wine/wine wine" ;;
    win64) candidates="wine64 /usr/lib/wine/wine64 wine" ;;
    *)
        echo "wine-run: WINE_RUN_ARCH must be win32 or win64, not $arch" >&2
        exit 2
        ;;
esac

wine=${WINE:-}
if [ -z "$wine" ]; then
    for candidate in $candidates; do
        if command -v "$candidate" >/dev/null 2>&1; then
            wine=$candidate
            break
        fi
    done
fi
[ -n "$wine" ] || {
    echo "wine-run: no wine/wine64 on PATH (needed to run Windows build tools)" >&2
    exit 1
}

root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$arch" = win32 ]; then
    export WINEPREFIX="${WINEPREFIX:-$root/build/wineprefix32}"
    [ -d "$WINEPREFIX" ] || export WINEARCH=win32
else
    export WINEPREFIX="${WINEPREFIX:-$root/build/wineprefix}"
fi
export WINEDEBUG="${WINEDEBUG:--all}"

if [ -n "${WINE_RUN_DLL_PATH:-}" ]; then
    winepath_list=""
    old_ifs=$IFS
    IFS=:
    for dir in $WINE_RUN_DLL_PATH; do
        [ -n "$dir" ] || continue
        winepath_list="${winepath_list:+$winepath_list;}Z:$(printf '%s' "$dir" | tr / '\\')"
    done
    IFS=$old_ifs
    export WINEPATH="$winepath_list${WINEPATH:+;$WINEPATH}"
fi

exec "$wine" "$@"
