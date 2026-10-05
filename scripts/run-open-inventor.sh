#!/bin/sh
# Run a built Open Inventor example or demo (Mentor 02.1.HelloCone, Toolmaker
# viewer, maze, ...), or a program built from examples/inventor/, through
# libx11-compat on the direct desktop-GL path: on screen by default, or
# --snapshot for a headless PNG. Build first with: make GLX=1 open-inventor
# (and make GLX=1 open-inventor-examples for examples/inventor/).
#
# Linux: gl* dispatch through GLVND's libOpenGL to the context libx11-compat
# creates on EGL, so the EGL provider must be GLVND's libEGL.so.1 (not a vendor
# libEGL loaded directly); headless snapshots use Mesa's surfaceless platform.
# macOS: Homebrew Mesa's libEGL (surfaceless) with gl* from the in-tree gl-only
# shim over Mesa's libGL, as for Motif's paperplane (scripts/run-paperplane.sh).
#
# Usage:
#   run-open-inventor.sh <name>                       open a live window
#   run-open-inventor.sh <name> --snapshot [out.png]  headless screenshot
set -eu

out=${OUT:-build}
apps="$out/open-inventor/build/apps"
# Programs built from examples/inventor/*.c++ (make GLX=1 open-inventor-examples).
user="$out/open-inventor/examples"

list_programs() {
    find "$user" "$apps" -type f -perm -u+x ! -path '*/CMakeFiles/*' 2>/dev/null \
        | sed 's|.*/||' | sort
}

if [ $# -eq 0 ]; then
    echo "usage: run-open-inventor.sh <name> [--snapshot [out.png]]"
    if [ -d "$apps" ]; then
        echo "available programs:"
        list_programs | sed 's/^/  /'
    else
        echo "nothing built yet; run: make GLX=1 open-inventor"
    fi
    exit 0
fi

name=$1
prog=$(find "$user" "$apps" -type f -perm -u+x -name "$name" ! -path '*/CMakeFiles/*' \
    2>/dev/null | head -n 1)
[ -n "$prog" ] || {
    echo "no Open Inventor program named $name under $user or $apps" >&2
    echo "available: $(list_programs | tr '\n' ' ')" >&2
    exit 1
}

case "$(uname -s)" in
    Darwin)
        mesa=$(brew --prefix mesa 2>/dev/null || echo /opt/homebrew/opt/mesa)
        export LIBX11_COMPAT_EGL="${LIBX11_COMPAT_EGL:-$mesa/lib/libEGL.dylib}"
        export EGL_PLATFORM="${EGL_PLATFORM:-surfaceless}"
        # Loader path for the runners (scripts/glx-snapshot.sh and
        # scripts/run-glx-window.sh prepend $out themselves).
        export GLX_EXTRA_LIBS="$PWD/$out/glshim:$mesa/lib${GLX_EXTRA_LIBS:+:$GLX_EXTRA_LIBS}"
        ;;
    *)
        export LIBX11_COMPAT_EGL="${LIBX11_COMPAT_EGL:-libEGL.so.1}"
        ;;
esac
# Font files under Open Inventor's names (make open-inventor builds them), so
# SoText2/SoText3 find a font instead of drawing nothing.
if [ -z "${FL_FONT_PATH:-}" ] && [ -d "$out/open-inventor/fonts" ]; then
    export FL_FONT_PATH="$PWD/$out/open-inventor/fonts"
fi

if [ "${2:-}" = "--snapshot" ]; then
    export EGL_PLATFORM="${EGL_PLATFORM:-surfaceless}"
    # SoXt maps its window and draws after the Xt event loop settles; give it
    # longer than a bare GLX demo before the capture.
    export GLX_SNAPSHOT_DELAY_MS="${GLX_SNAPSHOT_DELAY_MS:-4000}"
    exec scripts/glx-snapshot.sh "$prog" "${3:-/tmp/open-inventor-$name.png}"
fi

exec scripts/run-glx-window.sh "$prog"
