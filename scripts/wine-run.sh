#!/bin/sh
# Run a program from a Windows (make WINDOWS=1) build under Wine, as a
# command-line tool: stdin, stdout and the exit status pass through. It is the
# CMAKE_CROSSCOMPILING_EMULATOR of the Windows Open Inventor build, which runs
# its own build-time tools (ppp).
#
# WINE_RUN_DLL_PATH is a colon-separated list of Unix directories holding the
# DLLs the program loads; it becomes Wine's WINEPATH. WINEPREFIX defaults to
# build/wineprefix in this tree and WINEDEBUG to -all, so Wine's own
# diagnostics stay out of the program's output.
#
# Usage: wine-run.sh <program.exe> [args...]
set -eu

[ $# -ge 1 ] || {
    echo "usage: wine-run.sh <program.exe> [args...]" >&2
    exit 2
}

wine=${WINE:-}
if [ -z "$wine" ]; then
    for candidate in wine64 /usr/lib/wine/wine64 wine; do
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
export WINEPREFIX="${WINEPREFIX:-$root/build/wineprefix}"
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
