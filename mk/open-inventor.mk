# Open Inventor (https://github.com/aumuell/open-inventor): SGI's scene-graph
# toolkit, built unmodified (bar compat/open-inventor-patches) against the
# libx11-compat stack, with its Mentor/Toolmaker examples and demos.
#
# Everything X- and GL-shaped comes from this tree, never from the host:
#   - Xlib/Xt/Xi       libX11-compat, libXt-compat, libXi-compat
#   - Motif (SoXt)     the in-tree Motif build (mk/motif.mk)
#   - OpenGL           the direct desktop-GL path (mk/glx-direct.mk): gl* from
#                      GLVND's gl-only libOpenGL, glX* from libx11-compat, on the
#                      system Mesa desktop compatibility context; no gl4es
#   - GLU              mesa/glu, compiled here against libOpenGL
# scripts/open-inventor-cache.cmake pins those for Open Inventor's CMake build,
# and check-open-inventor audits the linked binaries for any host X11/GL library.
# libjpeg, freetype and iconv still come from the host.
#
# Linux + GLX=1 only (the direct path needs GLVND libOpenGL + Mesa EGL); a no-op
# elsewhere.

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
OI_GLU_LIB := $(OI_GLU_DIR)/libGLU.so
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
OI_JOBS ?= $(shell nproc 2>/dev/null || echo 2)

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

ifeq ($(GLX)/$(UNAME_S),1/Linux)

OI_OPENGL_LIB := $(shell $(CC) -print-file-name=libOpenGL.so 2>/dev/null)

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
$(OI_GLU_LIB): $(GLU_SRC_STAMP) $(GL_HDR_CACHE)/GL/gl.h $(GL_HDR_CACHE)/GL/glext.h
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
	        -I$(GL_HDR_CACHE) -c $(GLU_SRC_DIR)/src/$$src -o $$obj; \
	    objs="$$objs $$obj"; \
	done; \
	$(OI_CXX) -shared -Wl,-soname,libGLU.so.1 -Wl,--no-undefined \
	    -o $(OI_GLU_DIR)/libGLU.so.1 $$objs $(OI_OPENGL_LIB)
	$(Q)ln -sf libGLU.so.1 $@

# One include root holding X11/, Xm/ and GL/ so the CMake cache can point every
# X/GL include dir at it: the staged upstream X11 headers overlaid with the
# in-tree ones, the in-tree Motif headers, and the GL headers of the direct path
# (pristine gl.h/glext.h, our glx.h, GLU's glu.h).
$(OI_SYSROOT_STAMP): mk/open-inventor.mk $(UPSTREAM_HEADERS_STAMP) \
    $(MOTIF_STAGE_STAMP) $(GLU_SRC_STAMP) $(GL_HDR_CACHE)/GL/gl.h \
    $(GL_HDR_CACHE)/GL/glext.h
	@echo "  SYSROOT open-inventor"
	$(Q)rm -rf $(OI_SYSROOT)
	$(Q)mkdir -p $(OI_SYSROOT)/X11/extensions $(OI_SYSROOT)/Xm $(OI_SYSROOT)/GL
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
	             $(abspath $(MOTIF_BUILD_DIR))/lib/Xm/*.h; do \
	    ln -sf "$$h" "$(OI_SYSROOT)/Xm/$$(basename "$$h")"; \
	done
	$(Q)ln -sf $(abspath $(GL_HDR_CACHE))/GL/gl.h \
	    $(abspath $(GL_HDR_CACHE))/GL/glext.h $(abspath include)/GL/glx.h \
	    $(abspath $(GLU_SRC_DIR))/include/GL/glu.h $(OI_SYSROOT)/GL/
	$(Q)touch $@

OI_COMPAT_LIBS := $(TARGET) $(LIBXT_TARGET) $(XI_COMPAT_TARGET) \
    $(XEXT_COMPAT_TARGET) $(ICE_COMPAT_TARGET) $(SM_COMPAT_TARGET) $(MOTIF_LIBXM)

$(OI_CONFIG_STAMP): mk/open-inventor.mk scripts/open-inventor-cache.cmake \
    $(OI_SRC_STAMP) $(OI_SYSROOT_STAMP) $(OI_GLU_LIB) $(OI_COMPAT_LIBS)
	@echo "  CMAKE   open-inventor"
	$(Q)test -f "$(OI_OPENGL_LIB)" || { \
	    echo "  FAIL    no system libOpenGL.so (install libopengl-dev)" >&2; exit 1; }
	$(Q)rm -rf $(OI_BUILD_DIR)
	$(Q)mkdir -p $(OI_BUILD_DIR)
	$(Q)$(OI_CMAKE) -S $(OI_SRC_DIR) -B $(OI_BUILD_DIR) \
	    -DLIBX11_COMPAT_LIBDIR=$(abspath $(OUT)) \
	    -DLIBX11_COMPAT_SYSROOT=$(abspath $(OI_SYSROOT)) \
	    -DLIBX11_COMPAT_GLU=$(abspath $(OI_GLU_LIB)) \
	    -DLIBX11_COMPAT_OPENGL=$(OI_OPENGL_LIB) \
	    -C $(abspath scripts/open-inventor-cache.cmake) \
	    $(OI_CMAKE_FLAGS) > $(OI_LOG) 2>&1 || { \
	        echo "  FAIL    see $(OI_LOG)" >&2; tail -40 $(OI_LOG) >&2; exit 1; }
	$(Q)touch $@

$(OI_BUILD_STAMP): $(OI_CONFIG_STAMP)
	@echo "  MAKE    open-inventor"
	$(Q)env -u MAKEFLAGS -u MFLAGS $(OI_CMAKE) --build $(OI_BUILD_DIR) \
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
OI_USER_LIBS := $(OI_BUILD_DIR)/libSoXt/libInventorXt.so \
    $(OI_BUILD_DIR)/lib/libInventor.so $(MOTIF_LIBXM) $(LIBXT_TARGET) \
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
OI_AUDIT_BINS := lib/libInventor.so libSoXt/libInventorXt.so \
    apps/examples/Mentor/CXX/02.1.HelloCone apps/examples/Mentor/CXX/02.4.Examiner
# Examples rendered headless (scripts/run-open-inventor.sh --snapshot). The
# content check looks only at the GL canvas (OI_CHECK_REGION, x,y,w,h): the
# Examiner's Motif chrome alone would otherwise pass for a rendered frame.
OI_CHECK_EXAMPLES := 02.1.HelloCone 02.4.Examiner
OI_CHECK_REGION := 30,5,340,340

## Audit Open Inventor's links and render Mentor examples headless (direct GL)
check-open-inventor:
	$(Q)egl=$$($(CC) -print-file-name=libEGL.so.1); \
	    if [ ! -f "$$egl" ] || [ ! -f "$(OI_OPENGL_LIB)" ]; then \
	        echo "  SKIP    open-inventor (no system libEGL.so.1/libOpenGL.so; install libegl-dev libopengl-dev)"; \
	        exit 0; \
	    fi; \
	    set -e; \
	    $(MAKE) --no-print-directory open-inventor; \
	    outdir=$(abspath $(OUT)); \
	    for bin in $(OI_AUDIT_BINS); do \
	        echo "  CHECK   open-inventor links $$bin"; \
	        deps=$$(LD_LIBRARY_PATH=$$outdir ldd $(OI_BUILD_DIR)/$$bin | \
	            sed -n 's/^[[:space:]]*\([^[:space:]]*\) => \([^[:space:]]*\).*/\1 \2/p'); \
	        echo "$$deps" | grep -Eq '^lib(GL|GLX)\.so' && { \
	            echo "  FAIL    $$bin pulls libGL/libGLX:" >&2; echo "$$deps" >&2; exit 1; }; \
	        bad=$$(echo "$$deps" | \
	            grep -E '^lib(X11|Xt|Xi|Xext|Xmu|Xft|Xm|Mrm|ICE|SM|xcb|GLU)[.-]' | \
	            grep -v " $$outdir/" || true); \
	        if [ -n "$$bad" ]; then \
	            echo "  FAIL    $$bin resolves X/GL libraries outside $(OUT):" >&2; \
	            echo "$$bad" >&2; exit 1; \
	        fi; \
	    done; \
	    xt=$$(readelf -d $(OI_BUILD_DIR)/libSoXt/libInventorXt.so | \
	        sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); \
	    for lib in libXm.so.5 libXt-compat.so libX11-compat.so libXi-compat.so \
	               libOpenGL.so.0 libGLU.so.1; do \
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

else
open-inventor open-inventor-fetch open-inventor-examples:
	@echo "  SKIP    open-inventor (needs Linux + GLX=1)"
check-open-inventor:
	@echo "  SKIP    open-inventor (needs Linux + GLX=1)"
endif

open-inventor-clean:
	@echo "  CLEAN   open-inventor"
	$(Q)rm -rf $(OI_BUILD_ROOT)
