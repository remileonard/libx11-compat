# Open Inventor (https://github.com/aumuell/open-inventor): SGI's scene-graph
# toolkit, built unmodified (bar compat/open-inventor-patches) against the
# libx11-compat stack, with its Mentor/Toolmaker examples and demos.
#
# Everything X- and GL-shaped comes from this tree, never from the host:
#   - Xlib/Xt/Xi       libX11-compat, libXt-compat, libXi-compat
#   - Motif (SoXt)     the in-tree Motif build (mk/motif.mk)
#   - OpenGL           the direct desktop-GL path (mk/glx-direct.mk): gl* go
#                      straight to a Mesa desktop compatibility context, glX*
#                      come from libx11-compat; no gl4es. Linux: GLVND's
#                      gl-only libOpenGL. macOS: Homebrew Mesa through the
#                      gl-only re-export shim mk/motif.mk builds for GLw
#                      ($(MOTIF_GLSHIM_DIR)), which keeps Mesa's glX* out.
#   - GLU              mesa/glu, compiled here against that GL library
# scripts/open-inventor-cache.cmake pins those for Open Inventor's CMake build,
# and check-open-inventor audits the linked binaries for any host X11/GL library.
# libjpeg, freetype and iconv still come from the host.
#
# Linux + GLX=1, or macOS + GLX=1 with Homebrew Mesa (brew install mesa); a
# no-op elsewhere.

OI_URL := https://github.com/aumuell/open-inventor
OI_REVISION := 0813fbbc2e778d31c66325c8561404d26c659c52
OI_SRC_DIR := $(OUT)/upstream/open-inventor
OI_SRC_STAMP := $(OI_SRC_DIR)/.source-stamp
OI_PATCHES := $(sort $(wildcard compat/open-inventor-patches/*.patch))
# Snapshot the patch list so removing a patch (not only editing one) re-runs
# the checkout; rewritten only when the sorted list changes (see mk/motif.mk).
OI_PATCH_LIST_FILE := $(OUT)/upstream/.open-inventor-patch-list
$(shell mkdir -p $(dir $(OI_PATCH_LIST_FILE)); \
        new='$(sort $(notdir $(wildcard compat/open-inventor-patches/*.patch)))'; \
        old=$$(cat $(OI_PATCH_LIST_FILE) 2>/dev/null || true); \
        if [ "$$new" != "$$old" ]; then \
            printf '%s\n' "$$new" > $(OI_PATCH_LIST_FILE); \
        fi)

# GLU 9.0.3.
GLU_URL := https://gitlab.freedesktop.org/mesa/glu.git
GLU_REVISION := a2b96c7bba8db8fec3e02fb4227a7f7b02cabad1
GLU_SRC_DIR := $(OUT)/upstream/glu
GLU_SRC_STAMP := $(GLU_SRC_DIR)/.source-stamp

OI_GIT_Q := $(if $(filter 1,$(V)),,--quiet)
OI_BUILD_ROOT := $(OUT)/open-inventor
OI_GLU_DIR := $(OI_BUILD_ROOT)/glu
OI_SYSROOT := $(OI_BUILD_ROOT)/sysroot
OI_SYSROOT_STAMP := $(OI_SYSROOT)/.stamp
OI_BUILD_DIR := $(OI_BUILD_ROOT)/build
OI_CONFIG_STAMP := $(OI_BUILD_DIR)/.configure-stamp
OI_BUILD_STAMP := $(OI_BUILD_DIR)/.build-stamp
OI_LOG := $(abspath $(OI_BUILD_ROOT))/build.log
# Host fonts linked under the PostScript-style names libFL opens (see
# scripts/open-inventor-fonts.sh); run-open-inventor.sh sets FL_FONT_PATH here.
OI_FONT_DIR := $(OI_BUILD_ROOT)/fonts
OI_FONT_STAMP := $(OI_FONT_DIR)/.stamp
OI_CMAKE ?= cmake
# The tree builds with --no-builtin-variables, so CXX is unset unless the caller
# (or another fragment) sets it; GLU's NURBS code and examples/inventor need one.
OI_CXX = $(or $(CXX),$(OI_CXX_DEFAULT))
OI_CXX_DEFAULT := $(shell command -v clang++ >/dev/null 2>&1 && printf clang++ || printf c++)
OI_JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

OI_CMAKE_FLAGS := \
    -DCMAKE_BUILD_TYPE=Release \
    -DINVENTOR_MAN=OFF \
    -DINVENTOR_NODES=OFF \
    -DINVENTOR_DEMOS=ON \
    -DINVENTOR_SAMPLES=OFF \
    -DINVENTOR_EXAMPLES=ON

.PHONY: open-inventor open-inventor-fetch open-inventor-examples \
    check-open-inventor open-inventor-clean

define oi_git_checkout
	@echo "  GIT     $(1)"
	$(Q)mkdir -p $(dir $(2))
	$(Q)test -d $(2)/.git || git clone $(OI_GIT_Q) $(1) $(2)
	$(Q)cd $(2) && { git cat-file -e $(3)^{commit} 2>/dev/null || \
	    git fetch $(OI_GIT_Q) origin; } && \
	    git checkout $(OI_GIT_Q) --detach $(3) && \
	    git reset --hard $(OI_GIT_Q) $(3) >/dev/null && \
	    git clean $(OI_GIT_Q) -fdx >/dev/null
endef

# Per-platform pieces of the direct GL path; the rules below are shared.
#   OI_OPENGL_LIB   the gl-only GL library (gl*, no glX*) everything links
#   OI_OPENGL_DEP   make prerequisite that produces it (empty if from the host)
#   OI_EGL_LIB      the EGL provider libx11-compat loads (LIBX11_COMPAT_EGL)
#   OI_DSO          Open Inventor's shared-library suffix (CMake's choice)
OI_PLATFORM :=
OI_MOTIF_LIB := $(MOTIF_LIBXM)
OI_MOTIF_BUILD_DIR := $(MOTIF_BUILD_DIR)
OI_MOTIF_STAMP := $(MOTIF_STAGE_STAMP)
OI_GLU_BUILT := 1
ifeq ($(GLX)/$(UNAME_S),1/Linux)
OI_PLATFORM := linux
OI_OPENGL_LIB := $(shell $(CC) -print-file-name=libOpenGL.so 2>/dev/null)
OI_OPENGL_DEP :=
OI_EGL_LIB := $(shell $(CC) -print-file-name=libEGL.so.1 2>/dev/null)
OI_DSO := .so
OI_GLU_LIB := $(OI_GLU_DIR)/libGLU.so
OI_GLU_REAL := libGLU.so.1
OI_GLU_LINK = -shared -Wl,-soname,$(OI_GLU_REAL) -Wl,--no-undefined
OI_MISSING_HINT := install libegl-dev libopengl-dev
endif
# macOS: MOTIF_GLSHIM_DIR is set by mk/motif.mk only when Homebrew Mesa is
# installed. Mesa's libGL is monolithic (gl* and glX*), so Open Inventor links
# the shim that re-exports its gl* alone, exactly like GLw/paperplane.
ifeq ($(GLX)/$(UNAME_S),1/Darwin)
ifdef MOTIF_GLSHIM_DIR
OI_PLATFORM := darwin
OI_OPENGL_LIB := $(abspath $(MOTIF_GLSHIM_DIR))/libGL.dylib
OI_OPENGL_DEP := $(MOTIF_GL_PKGCONFIG)
OI_EGL_LIB := $(MOTIF_MESA_PREFIX)/lib/libEGL.dylib
OI_DSO := .dylib
OI_GLU_LIB := $(OI_GLU_DIR)/libGLU.dylib
OI_GLU_REAL := libGLU.1.dylib
OI_GLU_LINK = -dynamiclib -install_name $(abspath $(OI_GLU_DIR))/$(OI_GLU_REAL)
OI_MISSING_HINT := brew install mesa
# Homebrew's jpeg/freetype and its newer bison (Open Inventor's grammar).
OI_BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
OI_CMAKE_FLAGS += $(if $(OI_BREW_PREFIX),-DCMAKE_PREFIX_PATH=$(OI_BREW_PREFIX)) \
    $(if $(wildcard $(OI_BREW_PREFIX)/opt/bison/bin/bison),\
        -DBISON_EXECUTABLE=$(OI_BREW_PREFIX)/opt/bison/bin/bison)
endif
endif

# Windows (make WINDOWS=1, a MinGW-w64 cross build): gl* from the system
# opengl32, whose WGL contexts are the compatibility profile; glX* from
# libx11-compat, which runs GLX on WGL (src/egl-wgl.c); GLU from glu32; Motif
# is libXm.dll (mk/windows-motif.mk). FreeType and libjpeg are cross-built
# into the Windows sysroot (mk/windows-deps.mk), iconv comes from the POSIX
# shims. The build runs its own ppp tool, so CMake runs it through Wine
# (scripts/wine-run.sh). Data paths are relative to the program's directory
# (runtime prefix ".", example data in "data"), so the open-inventor-dist
# zip runs from wherever it is unpacked. Programs link compat/win32/binmode.c:
# the C runtime then opens files in binary mode by default, as on UNIX,
# instead of translating CR/LF and stopping at ^Z, which corrupts binary .iv
# files.
ifeq ($(GLX)/$(UNAME_S),1/Windows)
OI_PLATFORM := windows
OI_GLU_BUILT :=
OI_OPENGL_LIB := $(shell $(CC) -print-file-name=libopengl32.a 2>/dev/null)
OI_OPENGL_DEP :=
OI_DSO := .dll
OI_GLU_LIB := $(shell $(CC) -print-file-name=libglu32.a 2>/dev/null)
OI_MOTIF_LIB := $(MOTIF_WIN_LIBXM)
OI_MOTIF_BUILD_DIR := $(MOTIF_WIN_BUILD_DIR)
OI_MOTIF_STAMP := $(MOTIF_WIN_BUILD_STAMP)
OI_MISSING_HINT := install the MinGW-w64 toolchain
# MinGW's GNU C mode predefines WIN32, which would flip the X headers onto
# their native-Windows paths; the compat stack uses their POSIX view.
# _USE_MATH_DEFINES: M_PI and friends, which glibc's g++ exposes by default.
# APIENTRY: <GL/gl.h> otherwise includes <windows.h> to get it, and its macros
# (ERROR, Arc, ...) break Open Inventor's own names.
OI_WIN_CFLAGS := -UWIN32 -D_USE_MATH_DEFINES -DAPIENTRY=__stdcall $(WIN_ARCH_CFLAGS) \
    -I$(abspath compat/win32/include) -include x11compat-win32.h
OI_WIN_DEPS := $(WIN_FREETYPE_STAMP) $(WIN_JPEG_STAMP) $(WIN_POSIX_LIB) \
    $(WIN_BINMODE_OBJ) $(WIN_GL_CDECL_LIB)
# DLL directories for the build-time tools Wine runs: the compat stack, SDL3,
# and the MinGW C/C++ runtime.
OI_WIN_DLL_PATH := $(abspath $(OUT)):$(WIN_SYSROOT)/bin:$(abspath $(OI_BUILD_DIR))/lib:$(abspath $(OI_BUILD_DIR))/libimage:$(abspath $(OI_BUILD_DIR))/libFL:$(dir $(shell $(CXX) -print-file-name=libstdc++-6.dll 2>/dev/null)):$(dir $(shell $(CC) -print-file-name=libwinpthread-1.dll 2>/dev/null))
# Inputs of scripts/open-inventor-cache.cmake, so they go before its -C.
OI_CACHE_INPUTS := -DLIBX11_COMPAT_WINDOWS=ON \
    -DLIBX11_COMPAT_MOTIF=$(abspath $(MOTIF_WIN_LIBXM)).a \
    -DLIBX11_COMPAT_ICONV=$(abspath $(WIN_POSIX_LIB)) \
    -DLIBX11_COMPAT_ICONV_INCLUDE=$(abspath compat/win32/include) \
    $(if $(WIN_GL_CDECL_LIB),-DLIBX11_COMPAT_GL_CDECL=$(abspath $(WIN_GL_CDECL_LIB)))
OI_CMAKE_FLAGS += -DCMAKE_TOOLCHAIN_FILE=$(abspath $(WIN_CMAKE_TOOLCHAIN)) \
    -DCMAKE_CROSSCOMPILING_EMULATOR=$(abspath scripts/wine-run.sh) \
    "-DCMAKE_C_FLAGS=$(OI_WIN_CFLAGS)" "-DCMAKE_CXX_FLAGS=$(OI_WIN_CFLAGS)" \
    -DINVENTOR_DEMOS=ON \
    -DINVENTOR_RUNTIME_PREFIX=. -DINVENTOR_EXAMPLES_DATADIR=data \
    -DCMAKE_EXE_LINKER_FLAGS=$(abspath $(WIN_BINMODE_OBJ))
endif

ifdef OI_PLATFORM

$(OI_SRC_STAMP): mk/open-inventor.mk $(OI_PATCHES) $(OI_PATCH_LIST_FILE)
	$(call oi_git_checkout,$(OI_URL),$(OI_SRC_DIR),$(OI_REVISION))
	$(Q)set -e; for patch in $(abspath $(OI_PATCHES)); do \
	    echo "  PATCH   $$(basename $$patch)"; \
	    git -C $(OI_SRC_DIR) apply "$$patch"; \
	done
	$(Q)touch $@

$(GLU_SRC_STAMP): mk/open-inventor.mk
	$(call oi_git_checkout,$(GLU_URL),$(GLU_SRC_DIR),$(GLU_REVISION))
	$(Q)touch $@

## Fetch and patch the pinned Open Inventor and GLU source trees
open-inventor-fetch: $(OI_SRC_STAMP) $(GLU_SRC_STAMP)

# GLU, compiled straight from the source list in its meson.build (no meson
# needed) and linked against libOpenGL only, so it brings no libGL/libGLX.
# Windows uses the system glu32 instead (OI_GLU_BUILT empty).
ifdef OI_GLU_BUILT
$(OI_GLU_LIB): $(GLU_SRC_STAMP) $(GL_HDR_CACHE)/GL/gl.h $(GL_HDR_CACHE)/GL/glext.h \
    include/KHR/khrplatform.h $(OI_OPENGL_DEP)
	@echo "  CC      glu"
	$(Q)rm -rf $(OI_GLU_DIR)
	$(Q)mkdir -p $(OI_GLU_DIR)/obj
	$(Q)set -e; objs=; \
	for src in $$(sed -n "s/^ *'\(lib[^']*\.cc*\)',$$/\1/p" $(GLU_SRC_DIR)/src/meson.build); do \
	    obj=$(OI_GLU_DIR)/obj/$$(echo "$$src" | tr / _).o; \
	    case "$$src" in *.cc) cc='$(OI_CXX)';; *) cc='$(CC)';; esac; \
	    $$cc -O2 -fPIC -w -DLIBRARYBUILD -I$(GLU_SRC_DIR)/include \
	        -I$(GLU_SRC_DIR)/src/include \
	        -I$(GLU_SRC_DIR)/src/libnurbs/internals \
	        -I$(GLU_SRC_DIR)/src/libnurbs/interface \
	        -I$(GLU_SRC_DIR)/src/libnurbs/nurbtess \
	        -I$(GL_HDR_CACHE) -I$(abspath include) \
	        -c $(GLU_SRC_DIR)/src/$$src -o $$obj; \
	    objs="$$objs $$obj"; \
	done; \
	$(OI_CXX) $(OI_GLU_LINK) -o $(OI_GLU_DIR)/$(OI_GLU_REAL) $$objs \
	    $(OI_OPENGL_LIB)
	$(Q)ln -sf $(OI_GLU_REAL) $@
endif

# One include root holding X11/, Xm/ and GL/ so the CMake cache can point every
# X/GL include dir at it: the staged upstream X11 headers overlaid with the
# in-tree ones, the in-tree Motif headers, and the GL headers of the direct path
# (pristine gl.h/glext.h, our glx.h, GLU's glu.h, and the in-tree
# KHR/khrplatform.h glext.h includes: macOS has no system copy).
$(OI_SYSROOT_STAMP): mk/open-inventor.mk $(UPSTREAM_HEADERS_STAMP) \
    $(OI_MOTIF_STAMP) $(GLU_SRC_STAMP) $(GL_HDR_CACHE)/GL/gl.h \
    $(GL_HDR_CACHE)/GL/glext.h include/KHR/khrplatform.h
	@echo "  SYSROOT open-inventor"
	$(Q)rm -rf $(OI_SYSROOT)
	$(Q)mkdir -p $(OI_SYSROOT)/X11/extensions $(OI_SYSROOT)/Xm $(OI_SYSROOT)/GL
	$(Q)ln -sf $(abspath include)/KHR $(OI_SYSROOT)/KHR
	$(Q)for e in $(abspath $(OUT)/upstream/include)/X11/*; do \
	    b=$$(basename "$$e"); \
	    [ "$$b" = extensions ] && continue; \
	    ln -sf "$$e" "$(OI_SYSROOT)/X11/$$b"; \
	done
	$(Q)for e in $(abspath $(OUT)/upstream/include)/X11/extensions/* \
	             $(abspath include)/X11/extensions/*; do \
	    ln -sf "$$e" "$(OI_SYSROOT)/X11/extensions/$$(basename "$$e")"; \
	done
	$(Q)for e in $(abspath include)/X11/*; do \
	    b=$$(basename "$$e"); \
	    [ -e "$(OI_SYSROOT)/X11/$$b" ] || ln -sf "$$e" "$(OI_SYSROOT)/X11/$$b"; \
	done
	$(Q)for h in $(abspath $(MOTIF_SRC_DIR))/lib/Xm/*.h \
	             $(abspath $(OI_MOTIF_BUILD_DIR))/lib/Xm/*.h; do \
	    ln -sf "$$h" "$(OI_SYSROOT)/Xm/$$(basename "$$h")"; \
	done
	$(Q)ln -sf $(abspath $(GL_HDR_CACHE))/GL/gl.h \
	    $(abspath $(GL_HDR_CACHE))/GL/glext.h $(abspath include)/GL/glx.h \
	    $(abspath $(GLU_SRC_DIR))/include/GL/glu.h $(OI_SYSROOT)/GL/
	$(Q)touch $@

OI_COMPAT_LIBS := $(TARGET) $(LIBXT_TARGET) $(XI_COMPAT_TARGET) \
    $(XEXT_COMPAT_TARGET) $(ICE_COMPAT_TARGET) $(SM_COMPAT_TARGET) $(OI_MOTIF_LIB)

$(OI_CONFIG_STAMP): mk/open-inventor.mk scripts/open-inventor-cache.cmake \
    $(OI_SRC_STAMP) $(OI_SYSROOT_STAMP) $(OI_GLU_LIB) $(OI_COMPAT_LIBS) \
    $(OI_OPENGL_DEP) $(OI_WIN_DEPS)
	@echo "  CMAKE   open-inventor"
	$(Q)test -f "$(OI_OPENGL_LIB)" || { \
	    echo "  FAIL    no gl-only GL library $(OI_OPENGL_LIB) ($(OI_MISSING_HINT))" >&2; \
	    exit 1; }
	$(Q)rm -rf $(OI_BUILD_DIR)
	$(Q)mkdir -p $(OI_BUILD_DIR)
	$(Q)$(OI_CMAKE) -S $(OI_SRC_DIR) -B $(OI_BUILD_DIR) \
	    -DLIBX11_COMPAT_LIBDIR=$(abspath $(OUT)) \
	    -DLIBX11_COMPAT_SYSROOT=$(abspath $(OI_SYSROOT)) \
	    -DLIBX11_COMPAT_GLU=$(abspath $(OI_GLU_LIB)) \
	    -DLIBX11_COMPAT_OPENGL=$(OI_OPENGL_LIB) $(OI_CACHE_INPUTS) \
	    -C $(abspath scripts/open-inventor-cache.cmake) \
	    $(OI_CMAKE_FLAGS) > $(OI_LOG) 2>&1 || { \
	        echo "  FAIL    see $(OI_LOG)" >&2; tail -40 $(OI_LOG) >&2; exit 1; }
	$(Q)touch $@

$(OI_BUILD_STAMP): $(OI_CONFIG_STAMP)
	@echo "  MAKE    open-inventor"
	$(Q)env -u MAKEFLAGS -u MFLAGS \
	    $(if $(OI_WIN_DLL_PATH),WINE_RUN_DLL_PATH=$(OI_WIN_DLL_PATH) \
	        WINE_RUN_ARCH=$(if $(filter i686,$(WINDOWS_ARCH)),win32,win64)) \
	    $(OI_CMAKE) --build $(OI_BUILD_DIR) \
	    -j $(OI_JOBS) >> $(OI_LOG) 2>&1 || { \
	        echo "  FAIL    see $(OI_LOG)" >&2; tail -60 $(OI_LOG) >&2; exit 1; }
	$(Q)touch $@

$(OI_FONT_STAMP): scripts/open-inventor-fonts.sh
	@echo "  FONTS   open-inventor"
	$(Q)scripts/open-inventor-fonts.sh $(OI_FONT_DIR)
	$(Q)touch $@

## Build Open Inventor (libInventor, libInventorXt, Mentor/Toolmaker examples)
open-inventor: $(OI_BUILD_STAMP) $(OI_FONT_STAMP)

# Your own Open Inventor programs: every examples/inventor/<name>.c++ builds to
# $(OI_USER_DIR)/<name> against this tree, with the same headers, flags and
# libraries as the Mentor examples (scripts/run-open-inventor.sh finds them).
OI_USER_SRCS := $(wildcard examples/inventor/*.c++)
OI_USER_DIR := $(OI_BUILD_ROOT)/examples
OI_USER_BINS := $(patsubst examples/inventor/%.c++,$(OI_USER_DIR)/%,$(OI_USER_SRCS))
OI_USER_STD ?= c++98
OI_USER_CXXFLAGS := -O2 -std=$(OI_USER_STD) -Wno-write-strings \
    -fno-strict-aliasing -funsigned-char \
    $(foreach d,libSoXt/include lib/database/include lib/interaction/include \
        lib/nodekits/include,-I$(OI_SRC_DIR)/$(d)) \
    -isystem $(OI_SYSROOT)
OI_USER_LIBS := $(OI_BUILD_DIR)/libSoXt/libInventorXt$(OI_DSO) \
    $(OI_BUILD_DIR)/lib/libInventor$(OI_DSO) $(OI_MOTIF_LIB) $(LIBXT_TARGET) \
    $(XI_COMPAT_TARGET) $(TARGET) $(OI_GLU_LIB) $(OI_OPENGL_LIB)
OI_USER_RPATH := $(foreach d,$(OUT) $(OI_GLU_DIR) $(OI_BUILD_DIR)/libSoXt \
    $(OI_BUILD_DIR)/lib,-Wl$(comma)-rpath$(comma)$(abspath $(d)))

$(OI_USER_DIR)/%: examples/inventor/%.c++ $(OI_BUILD_STAMP) $(OI_FONT_STAMP)
	@mkdir -p $(@D)
	@echo "  CXX     $<"
	$(Q)$(OI_CXX) $(OI_USER_CXXFLAGS) $< -o $@ $(OI_USER_LIBS) $(OI_USER_RPATH)

## Build your own Open Inventor programs from examples/inventor/*.c++
open-inventor-examples: $(OI_USER_BINS)

# Binaries whose dynamic dependencies check-open-inventor audits.
OI_AUDIT_BINS := lib/libInventor$(OI_DSO) libSoXt/libInventorXt$(OI_DSO) \
    apps/examples/Mentor/CXX/02.1.HelloCone apps/examples/Mentor/CXX/02.4.Examiner
# Examples rendered headless (scripts/run-open-inventor.sh --snapshot). The
# content check looks only at the GL canvas (OI_CHECK_REGION, x,y,w,h): the
# Examiner's Motif chrome alone would otherwise pass for a rendered frame.
OI_CHECK_EXAMPLES := 02.1.HelloCone 02.4.Examiner
OI_CHECK_REGION := 30,5,340,340

# Link audit, per platform. Every X11/Motif/GL library a binary loads must come
# from this tree; Linux resolves with ldd, macOS reads otool -L install names
# (@rpath/ entries resolve through the build-tree rpaths). Windows has its own
# check below.
ifneq ($(OI_PLATFORM),windows)
ifeq ($(OI_PLATFORM),linux)
OI_XT_REQUIRED := libXm.so.5 libXt-compat libX11-compat libXi-compat \
    libOpenGL.so.0 libGLU.so.1
oi_list_needed = readelf -d $(1) | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'
define oi_audit_bin
deps=$$(LD_LIBRARY_PATH=$$outdir ldd $(1) | \
    sed -n 's/^[[:space:]]*\([^[:space:]]*\) => \([^[:space:]]*\).*/\1 \2/p'); \
echo "$$deps" | grep -Eq '^lib(GL|GLX)\.so' && { \
    echo "  FAIL    $(1) pulls libGL/libGLX:" >&2; echo "$$deps" >&2; exit 1; }; \
bad=$$(echo "$$deps" | \
    grep -E '^lib(X11|Xt|Xi|Xext|Xmu|Xft|Xm|Mrm|ICE|SM|xcb|GLU)[.-]' | \
    grep -v " $$outdir/" || true)
endef
else
OI_XT_REQUIRED := libXm libXt-compat libX11-compat libXi-compat \
    glshim/libGL.dylib $(OI_GLU_REAL)
oi_list_needed = otool -L $(1) | tail -n +2
define oi_audit_bin
deps=$$(otool -L $(1) | tail -n +2 | sed 's/^[[:space:]]*//; s/ (.*//'); \
bad=$$(echo "$$deps" | \
    grep -E '/lib(X11|Xt|Xi|Xext|Xmu|Xft|Xm|Mrm|ICE|SM|xcb|GL|GLX|GLU)[.-]' | \
    grep -v -e '^@rpath/' -e "^$$outdir/" || true)
endef
endif

## Audit Open Inventor's links and render Mentor examples headless (direct GL)
check-open-inventor:
	$(Q)if [ ! -f "$(OI_EGL_LIB)" ] || \
	    { [ ! -f "$(OI_OPENGL_LIB)" ] && [ -z "$(OI_OPENGL_DEP)" ]; }; then \
	    echo "  SKIP    open-inventor (no $(OI_EGL_LIB) / $(OI_OPENGL_LIB); $(OI_MISSING_HINT))"; \
	    exit 0; \
	fi; \
	set -e; \
	$(MAKE) --no-print-directory open-inventor; \
	outdir=$(abspath $(OUT)); \
	for bin in $(OI_AUDIT_BINS); do \
	    echo "  CHECK   open-inventor links $$bin"; \
	    $(call oi_audit_bin,$(OI_BUILD_DIR)/$$bin); \
	    if [ -n "$$bad" ]; then \
	        echo "  FAIL    $$bin loads X/GL libraries from outside $(OUT):" >&2; \
	        echo "$$bad" >&2; exit 1; \
	    fi; \
	done; \
	xt=$$($(call oi_list_needed,$(OI_BUILD_DIR)/libSoXt/libInventorXt$(OI_DSO))); \
	for lib in $(OI_XT_REQUIRED); do \
	    echo "$$xt" | grep -q "$$lib" || { \
	        echo "  FAIL    libInventorXt does not link $$lib" >&2; exit 1; }; \
	done; \
	for ex in $(OI_CHECK_EXAMPLES); do \
	    echo "  CHECK   open-inventor render $$ex"; \
	    OUT=$(OUT) scripts/run-open-inventor.sh $$ex \
	        --snapshot $(OUT)/open-inventor-$$ex.png; \
	    $(PYTHON) scripts/assert-image-content.py \
	        $(OUT)/open-inventor-$$ex.png 0.10 --region $(OI_CHECK_REGION); \
	done
endif

ifeq ($(OI_PLATFORM),windows)
# Distribution: the Mentor examples, Open Inventor's tools (ivview, ivcat, ...)
# and demos (SceneViewer, maze, drop, ...) with every DLL they load, the
# example data, the models/demo data/help from Open Inventor's own install,
# and the fonts (copied, with their licenses). The install is flattened so the
# programs sit next to the DLLs and the relative paths compiled in (data,
# ./share/inventor/...) resolve from the folder. chesschairs.iv, which the
# install would convert to binary with the host's ivcat, ships as ASCII.
# Debug information is stripped from the shipped binaries.
OI_DIST_NAME := open-inventor-$(notdir $(OUT))
OI_DIST_DIR := $(OUT)/dist/$(OI_DIST_NAME)
OI_DIST_ZIP := $(OUT)/dist/$(OI_DIST_NAME).zip
OI_DIST_DLLS := $(OI_COMPAT_LIBS) $(MOTIF_WIN_LIBMRM) $(LIBXPM_TARGET) \
    $(XMU_COMPAT_TARGET) $(XINERAMA_COMPAT_TARGET) $(XFT_COMPAT_TARGET) \
    $(OI_BUILD_DIR)/lib/libInventor.dll $(OI_BUILD_DIR)/libSoXt/libInventorXt.dll \
    $(WIN_SYSROOT)/bin/SDL3.dll $(WIN_SYSROOT)/bin/SDL3_ttf.dll
OI_DIST_RUNTIME := libwinpthread-1.dll $(WIN_LIBGCC_DLL) libstdc++-6.dll

.PHONY: open-inventor-dist
## Zip the Windows Open Inventor examples with their DLLs, data and fonts
open-inventor-dist: $(OI_DIST_ZIP)

$(OI_DIST_ZIP): $(OI_BUILD_STAMP) $(OI_FONT_STAMP) \
    scripts/windows-open-inventor-readme.txt
	@echo "  DIST    $@"
	$(Q)rm -rf $(OI_DIST_DIR) && mkdir -p $(OI_DIST_DIR)/share/inventor/fonts
	$(Q)cp $(OI_DIST_DLLS) $(OI_BUILD_DIR)/apps/examples/Mentor/CXX/*.exe \
	    $(OI_DIST_DIR)/
	$(Q)for dll in $(OI_DIST_RUNTIME); do \
	    path=$$($(CXX) -print-file-name=$$dll); \
	    [ -f "$$path" ] || { echo "  FAIL    $$dll not found by $(CXX)" >&2; exit 1; }; \
	    cp "$$path" $(OI_DIST_DIR)/; \
	done
	$(Q)cp -R $(OI_SRC_DIR)/apps/examples/data $(OI_DIST_DIR)/data
	$(Q)stage=$(abspath $(OI_DIST_DIR))/.stage; rm -rf "$$stage"; \
	mkdir -p "$$stage" && cd "$$stage" && \
	$(OI_CMAKE) --install $(abspath $(OI_BUILD_DIR)) --prefix "$$stage" \
	    >> $(OI_LOG) 2>&1 || { echo "  FAIL    see $(OI_LOG)" >&2; exit 1; }
	$(Q)stage=$(OI_DIST_DIR)/.stage; \
	cp "$$stage"/bin/*.exe $(OI_DIST_DIR)/ && \
	cp -f "$$stage"/libexec/inventor/*.exe $(OI_DIST_DIR)/ && \
	cp -R "$$stage"/share/inventor/data "$$stage"/share/inventor/help \
	    $(OI_DIST_DIR)/share/inventor/ && \
	cp $(OI_SRC_DIR)/data/models/scenes/chesschairs.iv.asc \
	    $(OI_DIST_DIR)/share/inventor/data/models/scenes/chesschairs.iv && \
	rm -rf "$$stage"
	$(Q)for font in $(OI_FONT_DIR)/*; do \
	    cp -L "$$font" $(OI_DIST_DIR)/share/inventor/fonts/; \
	    file=$$(readlink -f "$$font"); \
	    pkg=$$(dpkg -S "$$file" 2>/dev/null | cut -d: -f1); \
	    [ -z "$$pkg" ] || [ ! -f /usr/share/doc/$$pkg/copyright ] || \
	        cp /usr/share/doc/$$pkg/copyright \
	            $(OI_DIST_DIR)/share/inventor/fonts/LICENSE-$$pkg.txt; \
	done
	$(Q)$(call win_copy_dist_fonts,$(OI_DIST_DIR))
	$(Q)$(MINGW_TRIPLE)-strip --strip-debug $(OI_DIST_DIR)/*.exe $(OI_DIST_DIR)/*.dll
	$(Q)cp scripts/windows-open-inventor-readme.txt $(OI_DIST_DIR)/README.txt
	$(Q)rm -f $@ && cd $(dir $@) && zip -qr $(notdir $@) $(OI_DIST_NAME)

OI_WINE_ARCH := $(if $(filter i686,$(WINDOWS_ARCH)),win32,win64)

## Audit the Windows dist's DLL imports and render Mentor examples on screen
## under Wine (Xvfb)
check-open-inventor: $(OI_DIST_ZIP)
	$(Q)set -e; cd $(OI_DIST_DIR); \
	for bin in *.exe *.dll; do \
	    for dll in $$($(MINGW_TRIPLE)-objdump -p "$$bin" | \
	        sed -n 's/^[[:space:]]*DLL Name: //p' | grep -Ei '^(lib|SDL)'); do \
	        [ -f "$$dll" ] || { \
	            echo "  FAIL    $$bin imports $$dll, which the dist lacks" >&2; \
	            exit 1; }; \
	    done; \
	done
	@echo "  CHECK   open-inventor dist: every lib*/SDL* import is shipped"
	$(Q)set -e; for ex in $(OI_CHECK_EXAMPLES); do \
	    echo "  CHECK   open-inventor render $$ex (Wine, on screen)"; \
	    WINE_RUN_ARCH=$(OI_WINE_ARCH) scripts/wine-screenshot.sh \
	        $(OI_DIST_DIR) $$ex.exe $(OUT)/open-inventor-$$ex.bmp 20; \
	    $(PYTHON) scripts/assert-image-content.py \
	        $(OUT)/open-inventor-$$ex.bmp 0.10 --region $(OI_CHECK_REGION); \
	done
endif

else
open-inventor open-inventor-fetch open-inventor-examples:
	@echo "  SKIP    open-inventor (needs GLX=1 on Linux, or on macOS with Homebrew Mesa)"
check-open-inventor:
	@echo "  SKIP    open-inventor (needs GLX=1 on Linux, or on macOS with Homebrew Mesa)"
endif

open-inventor-clean:
	@echo "  CLEAN   open-inventor"
	$(Q)rm -rf $(OI_BUILD_ROOT)
