# Windows target (MinGW-w64 cross build).
#
# WINDOWS=1 retargets the build to the MinGW-w64 cross compiler and the native
# SDL3 backend, producing libX11-compat.dll (plus its import library) and
# Windows .exe examples. The SDL3, SDL3_ttf and pixman it links are cross-built
# into a private sysroot by mk/windows-deps.mk, so nothing from the build
# machine's own SDL or pixman can leak into the Windows binaries.
#
# Scope: the core Xlib library with GLX, the toolkit DLLs (mk/libxt.mk,
# mk/xcompat-libs.mk, ...), Motif (mk/windows-motif.mk) and the bundled
# examples. GLX runs on WGL (opengl32.dll) through the EGL emulation in
# src/egl-wgl.c.
#
# This fragment is included right after mk/wasm.mk, before mk/toolchain.mk
# applies the native compiler default, so CC/OUT/TARGET land first.

ifeq ($(WINDOWS),1)

MINGW_TRIPLE ?= x86_64-w64-mingw32
# Prefer the posix-threads flavour of the Debian/Ubuntu MinGW packages
# (winpthreads: pthread_*, clock_gettime, nanosleep); plain <triple>-gcc is
# that flavour on most other distributions.
MINGW_CC_DEFAULT := $(shell command -v $(MINGW_TRIPLE)-gcc-posix >/dev/null 2>&1 \
    && printf '%s' $(MINGW_TRIPLE)-gcc-posix || printf '%s' $(MINGW_TRIPLE)-gcc)
MINGW_CXX_DEFAULT := $(shell command -v $(MINGW_TRIPLE)-g++-posix >/dev/null 2>&1 \
    && printf '%s' $(MINGW_TRIPLE)-g++-posix || printf '%s' $(MINGW_TRIPLE)-g++)
CC := $(MINGW_CC_DEFAULT)
CXX := $(MINGW_CXX_DEFAULT)
AR := $(MINGW_TRIPLE)-ar
NM := $(MINGW_TRIPLE)-nm
# Build-machine helpers (libXt's makestrs, ...) stay native.
HOST_CC ?= cc

# XCB links native-only libraries.
override XCB := 0
override SDL_BACKEND := sdl3

OUT ?= build/win64
TARGET := $(OUT)/libX11-compat.dll
SHLIB := .dll
EXE := .exe

# Cross-built SDL3 / SDL3_ttf / pixman (mk/windows-deps.mk). Deterministic
# paths, so no parse-time pkg-config against a not-yet-built .pc is needed.
WIN_SYSROOT := $(abspath $(OUT))/win-sysroot
WIN_DEP_DIR := $(OUT)/win-deps
WIN_CMAKE_TOOLCHAIN := $(WIN_DEP_DIR)/mingw-toolchain.cmake
# Built only for Open Inventor (mk/open-inventor.mk).
WIN_FREETYPE_STAMP := $(WIN_DEP_DIR)/.freetype-stamp
WIN_JPEG_STAMP := $(WIN_DEP_DIR)/.jpeg-stamp

# The toolkit libraries (libXt, Xmu, Motif, ...) compile upstream sources with
# their own flag sets; they need the same POSIX shim headers as the core, and
# link the shim objects from a static archive (mk/windows-deps.mk) rather than
# reaching into libX11-compat.dll, whose exports stay the Xlib API.
WIN_COMPAT_CPPFLAGS := -I$(abspath compat/win32/include) -include x11compat-win32.h
WIN_POSIX_LIB := $(OUT)/libwin32-posix.a
WIN_POSIX_LDLIBS := $(WIN_POSIX_LIB) -lws2_32 -lpsapi

endif
