# Windows (WINDOWS=1) dependencies: SDL3, SDL3_ttf (with its vendored
# FreeType) and pixman, cross-built with MinGW-w64 into $(WIN_SYSROOT). Pinned
# like the other upstream fetches; SDL3/SDL3_ttf build as DLLs (shipped next to
# the executables), pixman as a static archive linked into libX11-compat.dll.
#
# Needs cmake + ninja (SDL3, SDL3_ttf) and autoconf/automake/libtool (pixman)
# on the build machine, plus the MinGW-w64 cross compiler (mk/windows.mk).

ifeq ($(WINDOWS),1)

WIN_SDL3_URL := https://github.com/libsdl-org/SDL.git
WIN_SDL3_REVISION := f5e5f6588921eed3d7d048ce43d9eb1ff0da0ffc # release-3.2.30
WIN_SDL3_TTF_URL := https://github.com/libsdl-org/SDL_ttf.git
WIN_SDL3_TTF_REVISION := a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b # release-3.2.2

WIN_DEP_DIR := $(OUT)/win-deps
WIN_DEPS_STAMP := $(WIN_SYSROOT)/.deps-stamp
WIN_CMAKE_TOOLCHAIN := $(WIN_DEP_DIR)/mingw-toolchain.cmake
WIN_GIT_Q := $(if $(filter 1,$(V)),,--quiet)
WIN_LOG := $(abspath $(WIN_DEP_DIR))/build.log

$(WIN_DEP_DIR):
	@mkdir -p $@

$(WIN_CMAKE_TOOLCHAIN): mk/windows-deps.mk | $(WIN_DEP_DIR)
	$(Q){ \
	    echo 'set(CMAKE_SYSTEM_NAME Windows)'; \
	    echo 'set(CMAKE_SYSTEM_PROCESSOR x86_64)'; \
	    echo 'set(CMAKE_C_COMPILER $(CC))'; \
	    echo 'set(CMAKE_CXX_COMPILER $(CXX))'; \
	    echo 'set(CMAKE_RC_COMPILER $(MINGW_TRIPLE)-windres)'; \
	    echo 'set(CMAKE_FIND_ROOT_PATH /usr/$(MINGW_TRIPLE) $(WIN_SYSROOT))'; \
	    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)'; \
	    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)'; \
	    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)'; \
	    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)'; \
	} > $@

# $(1) url, $(2) checkout dir, $(3) revision.
define win_git_checkout
	$(Q)test -d $(2)/.git || git clone $(WIN_GIT_Q) --filter=blob:none $(1) $(2)
	$(Q)cd $(2) && { git cat-file -e $(3)^{commit} 2>/dev/null || \
	    git fetch $(WIN_GIT_Q) origin; } && \
	    git checkout $(WIN_GIT_Q) --detach $(3) && \
	    git reset --hard $(WIN_GIT_Q) $(3) >/dev/null
endef

define win_fail
{ echo "  FAIL    see $(WIN_LOG)" >&2; tail -40 $(WIN_LOG) >&2; exit 1; }
endef

WIN_SDL3_STAMP := $(WIN_DEP_DIR)/.sdl3-stamp
WIN_SDL3_TTF_STAMP := $(WIN_DEP_DIR)/.sdl3-ttf-stamp
WIN_PIXMAN_STAMP := $(WIN_DEP_DIR)/.pixman-stamp

$(WIN_SDL3_STAMP): $(WIN_CMAKE_TOOLCHAIN)
	@echo "  WINDEP  SDL3"
	$(call win_git_checkout,$(WIN_SDL3_URL),$(WIN_DEP_DIR)/SDL,$(WIN_SDL3_REVISION))
	$(Q)cmake -S $(WIN_DEP_DIR)/SDL -B $(WIN_DEP_DIR)/SDL/build -G Ninja \
	    -DCMAKE_TOOLCHAIN_FILE=$(abspath $(WIN_CMAKE_TOOLCHAIN)) \
	    -DCMAKE_INSTALL_PREFIX=$(WIN_SYSROOT) -DCMAKE_BUILD_TYPE=Release \
	    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF \
	    -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)ninja -C $(WIN_DEP_DIR)/SDL/build install >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)touch $@

# SDLTTF_VENDORED builds SDL3_ttf's bundled FreeType into the DLL; HarfBuzz and
# the SVG/color-emoji backends are not needed for the core-font path.
$(WIN_SDL3_TTF_STAMP): $(WIN_SDL3_STAMP)
	@echo "  WINDEP  SDL3_ttf"
	$(call win_git_checkout,$(WIN_SDL3_TTF_URL),$(WIN_DEP_DIR)/SDL_ttf,$(WIN_SDL3_TTF_REVISION))
	$(Q)git -C $(WIN_DEP_DIR)/SDL_ttf submodule update $(WIN_GIT_Q) --init \
	    --depth 1 external/freetype >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)cmake -S $(WIN_DEP_DIR)/SDL_ttf -B $(WIN_DEP_DIR)/SDL_ttf/build -G Ninja \
	    -DCMAKE_TOOLCHAIN_FILE=$(abspath $(WIN_CMAKE_TOOLCHAIN)) \
	    -DCMAKE_INSTALL_PREFIX=$(WIN_SYSROOT) -DCMAKE_PREFIX_PATH=$(WIN_SYSROOT) \
	    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
	    -DSDLTTF_VENDORED=ON -DSDLTTF_HARFBUZZ=OFF -DSDLTTF_PLUTOSVG=OFF \
	    -DSDLTTF_SAMPLES=OFF >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)ninja -C $(WIN_DEP_DIR)/SDL_ttf/build install >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)touch $@

# pixman reuses the pinned, digest-checked tarball of the wasm leg
# (mk/wasm-deps.mk: PIXMAN_VERSION, PIXMAN_WASM_URLS, PIXMAN_WASM_SHA256).
WIN_PIXMAN_TARBALL := $(WIN_DEP_DIR)/pixman-$(PIXMAN_VERSION).tar.gz
WIN_PIXMAN_SRC := $(WIN_DEP_DIR)/pixman-$(PIXMAN_VERSION)

$(WIN_PIXMAN_TARBALL): mk/windows-deps.mk | $(WIN_DEP_DIR)
	@echo "  FETCH   pixman-$(PIXMAN_VERSION)"
	$(Q)for url in $(PIXMAN_WASM_URLS); do \
	    curl -fsSL --max-time 120 -o $@.tmp "$$url" || continue; \
	    got=$$(sha256sum $@.tmp | cut -d' ' -f1); \
	    [ "$$got" = "$(PIXMAN_WASM_SHA256)" ] && { mv $@.tmp $@; exit 0; }; \
	done; rm -f $@.tmp; \
	echo "no pixman-$(PIXMAN_VERSION) mirror returned the expected digest" >&2; exit 1

$(WIN_PIXMAN_STAMP): $(WIN_PIXMAN_TARBALL)
	@echo "  WINDEP  pixman-$(PIXMAN_VERSION)"
	$(Q)rm -rf $(WIN_PIXMAN_SRC) && mkdir -p $(WIN_PIXMAN_SRC)
	$(Q)tar xzf $< -C $(WIN_PIXMAN_SRC) --strip-components=1
	$(Q)cd $(WIN_PIXMAN_SRC) && NOCONFIGURE=1 ./autogen.sh >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)cd $(WIN_PIXMAN_SRC) && ./configure --host=$(MINGW_TRIPLE) CC=$(CC) \
	    --prefix=$(WIN_SYSROOT) --enable-static --disable-shared \
	    --disable-gtk --disable-libpng --disable-openmp CFLAGS=-O2 \
	    >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)$(MAKE) -C $(WIN_PIXMAN_SRC) SUBDIRS=pixman install >> $(WIN_LOG) 2>&1 || $(win_fail)
	$(Q)touch $@

$(WIN_DEPS_STAMP): $(WIN_SDL3_TTF_STAMP) $(WIN_PIXMAN_STAMP)
	$(Q)touch $@

.PHONY: windows-deps
## Cross-build the Windows dependencies (SDL3, SDL3_ttf, pixman; WINDOWS=1)
windows-deps: $(WIN_DEPS_STAMP)

# The POSIX shims (poll, dlfcn, iconv, regex, sigaction, ...) as a static archive for the
# toolkit DLLs; the core links the same objects directly.
WIN_POSIX_OBJS := $(addprefix $(OUT)/compat/win32/,dlfcn.o iconv.o poll.o \
    posix.o regex.o signal.o)
$(WIN_POSIX_LIB): $(WIN_POSIX_OBJS)
	@echo "  AR      $@"
	$(Q)rm -f $@ && $(AR) rcs $@ $^

# Every first-party object needs SDL3/pixman headers from the sysroot.
$(OBJS): | $(WIN_DEPS_STAMP)

# Distribution: the examples with every DLL they load (libX11-compat, SDL3,
# SDL3_ttf, and the MinGW runtime), ready to unzip and run on Windows.
WIN_DIST_DIR := $(OUT)/dist/libx11-compat-win64
WIN_DIST_ZIP := $(OUT)/dist/libx11-compat-win64.zip
WIN_RUNTIME_DLLS := libwinpthread-1.dll libgcc_s_seh-1.dll

.PHONY: windows-dist
## Stage the Windows examples + DLLs and zip them (WINDOWS=1)
windows-dist: $(WIN_DIST_ZIP)

$(WIN_DIST_ZIP): $(TARGET) $(EXAMPLE_BINS) $(X11PERF_BIN) $(WIN_DEPS_STAMP) \
    scripts/windows-dist-readme.txt
	@echo "  DIST    $@"
	$(Q)rm -rf $(WIN_DIST_DIR) && mkdir -p $(WIN_DIST_DIR)
	$(Q)cp $(TARGET) $(EXAMPLE_BINS) $(X11PERF_BIN) \
	    $(WIN_SYSROOT)/bin/SDL3.dll $(WIN_SYSROOT)/bin/SDL3_ttf.dll $(WIN_DIST_DIR)/
	$(Q)for dll in $(WIN_RUNTIME_DLLS); do \
	    path=$$($(CC) -print-file-name=$$dll); \
	    [ -f "$$path" ] || { echo "  FAIL    $$dll not found by $(CC)" >&2; exit 1; }; \
	    cp "$$path" $(WIN_DIST_DIR)/; \
	done
	$(Q)cp scripts/windows-dist-readme.txt $(WIN_DIST_DIR)/README.txt
	$(Q)rm -f $@ && cd $(dir $@) && zip -qr $(notdir $@) $(notdir $(WIN_DIST_DIR))

endif
