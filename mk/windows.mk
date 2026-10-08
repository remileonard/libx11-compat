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

# WINDOWS_ARCH=i686 builds 32-bit Windows binaries into build/win32. That is
# the target for unmodified legacy X11/Motif/Open Inventor code: on 64-bit
# Windows long stays 32 bits (LLP64) while pointers grow, so code written for
# 32-bit or LP64 UNIX that keeps pointers in long (Open Inventor's SbDict
# keys, (unsigned long) casts) truncates them. On 32-bit Windows long and
# pointers are both 32 bits, as on the UNIX systems that code came from.
WINDOWS_ARCH ?= x86_64
ifeq ($(WINDOWS_ARCH),i686)
MINGW_TRIPLE ?= i686-w64-mingw32
OUT ?= build/win32
WIN_CMAKE_PROCESSOR := x86
# 32-bit MinGW unwinds with DWARF-2 tables (libgcc_s_dw2), 64-bit with SEH.
WIN_LIBGCC_DLL := libgcc_s_dw2-1.dll
else ifeq ($(WINDOWS_ARCH),x86_64)
MINGW_TRIPLE ?= x86_64-w64-mingw32
OUT ?= build/win64
WIN_CMAKE_PROCESSOR := x86_64
WIN_LIBGCC_DLL := libgcc_s_seh-1.dll
else
$(error WINDOWS_ARCH must be x86_64 or i686, not '$(WINDOWS_ARCH)')
endif
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

# Wine prefix for running the build's programs: a 32-bit build needs a win32
# prefix (scripts/wine-run.sh creates it on first use).
WIN_WINEPREFIX := $(abspath build)/wineprefix$(if $(filter i686,$(WINDOWS_ARCH)),32)

endif
