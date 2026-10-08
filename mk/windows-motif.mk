# Windows (WINDOWS=1) Motif: libXm.dll and libMrm.dll cross-built with
# MinGW-w64 against the compat toolkit DLLs.
#
# Reuses the arch-independent front of mk/motif.mk (source clone, patches,
# autoreconf: MOTIF_SRC_STAMP / MOTIF_AUTOGEN_STAMP) and, like the wasm leg
# (mk/wasm-motif.mk), cross-configures Motif to build static archives with
# the build machine's makestrs. Each archive is then relinked whole into a
# DLL here rather than through libtool, whose MinGW DLL path wants
# -no-undefined plumbed through every Makefile.am. Exports are automatic
# (every global), and the widget-class data the clients reference resolves
# through MinGW's auto-import, as for the other toolkit DLLs.
#
# configure appends the pkg-config libraries to LDFLAGS, ahead of the
# conftest object; PE linkers resolve left to right, so its Xutf8TextExtents
# probe cannot see libX11-compat and is answered from the cache instead
# (src/ implements the Xutf8 family).

ifeq ($(WINDOWS),1)

MOTIF_WIN_BUILD_DIR := $(OUT)/win-motif
MOTIF_WIN_CONFIG_STAMP := $(MOTIF_WIN_BUILD_DIR)/.configure-stamp
MOTIF_WIN_BUILD_STAMP := $(MOTIF_WIN_BUILD_DIR)/.build-stamp
MOTIF_WIN_HOST_MAKESTRS := $(MOTIF_WIN_BUILD_DIR)/config/makestrs
MOTIF_WIN_LOG := $(abspath $(MOTIF_WIN_BUILD_DIR))/build.log
MOTIF_WIN_LIBXM := $(OUT)/libXm.dll
MOTIF_WIN_LIBMRM := $(OUT)/libMrm.dll

MOTIF_WIN_DEPS := $(TARGET) $(LIBXT_TARGET) $(LIBXPM_TARGET) $(XINERAMA_COMPAT_TARGET) \
    $(XEXT_COMPAT_TARGET) $(XMU_COMPAT_TARGET) $(WIN_POSIX_LIB)
# gcc's GNU modes predefine WIN32 (not just _WIN32), which flips the X headers
# onto their native-Windows paths (Xthreads.h pulls <windows.h>, whose
# LoadImage/... collide with Motif's own names). The compat stack is built
# against the POSIX view of those headers (libXt uses -std=c99, which leaves
# WIN32 undefined), so Motif is too.
MOTIF_WIN_CPPFLAGS := -UWIN32 -include stdlib.h $(WIN_COMPAT_CPPFLAGS)
MOTIF_WIN_CFLAGS := -O2 -g

$(MOTIF_WIN_CONFIG_STAMP): $(MOTIF_AUTOGEN_STAMP) $(PKGCONFIG_FILES) \
    $(MOTIF_WIN_DEPS) mk/windows-motif.mk
	@echo "  CONFIG  motif (windows)"
	$(Q)rm -rf $(MOTIF_WIN_BUILD_DIR) && mkdir -p $(MOTIF_WIN_BUILD_DIR)
	$(Q)cd $(MOTIF_WIN_BUILD_DIR) && \
	    $(abspath $(MOTIF_SRC_DIR))/configure \
	    --host=$(MINGW_TRIPLE) \
	    --enable-static --disable-shared \
	    --disable-glw --disable-demos --disable-tests \
	    --disable-dependency-tracking \
	    --without-xft --with-jpeg=no --with-png=no \
	    --with-xrandr=no --with-xrender=no --with-xcursor=no --with-xinerama=no \
	    PKG_CONFIG_LIBDIR=$(abspath $(PKGCONFIG_DIR)) PKG_CONFIG_PATH= \
	    CC=$(CC) CPP="$(CC) -E" CPPFLAGS="$(MOTIF_WIN_CPPFLAGS)" \
	    CFLAGS="$(MOTIF_WIN_CFLAGS)" LDFLAGS="-L$(abspath $(OUT))" \
	    LIBS="-lwin32-posix -lws2_32 -lpsapi" \
	    YACC="$(MOTIF_YACC)" \
	    ac_cv_search_Xutf8TextExtents='none required' \
	    $(call motif_log_redirect,$(MOTIF_WIN_LOG))
	$(Q)touch $@

$(MOTIF_WIN_BUILD_STAMP): $(MOTIF_WIN_CONFIG_STAMP)
	@echo "  CC      motif config/makestrs (host)"
	$(Q)mkdir -p $(dir $(MOTIF_WIN_HOST_MAKESTRS))
	$(Q)$(HOST_CC) -O2 -I$(OUT)/upstream/include -Iinclude \
	    -o $(MOTIF_WIN_HOST_MAKESTRS) $(MOTIF_SRC_DIR)/config/makestrs.c
	@touch $(MOTIF_WIN_HOST_MAKESTRS)
	@echo "  MAKE    motif lib/Xm (windows)"
	$(Q)$(MOTIF_SUBMAKE) -C $(MOTIF_WIN_BUILD_DIR)/lib/Xm \
	    $(call motif_log_redirect,$(MOTIF_WIN_LOG))
	@echo "  MAKE    motif lib/Mrm (windows)"
	$(Q)$(MOTIF_SUBMAKE) -C $(MOTIF_WIN_BUILD_DIR)/lib/Mrm \
	    $(call motif_log_redirect,$(MOTIF_WIN_LOG))
	$(Q)touch $@

$(MOTIF_WIN_LIBXM): $(MOTIF_WIN_BUILD_STAMP)
	@echo "  LD      $@"
	$(Q)$(CC) -shared -o $@ -Wl,--out-implib,$@.a \
	    -Wl,--whole-archive $(MOTIF_WIN_BUILD_DIR)/lib/Xm/.libs/libXm.a \
	    -Wl,--no-whole-archive -L$(OUT) -lXmu-compat -lXext-compat -lXinerama-compat \
	    -lXpm-compat -lXt-compat -lX11-compat $(WIN_POSIX_LDLIBS) -lm

$(MOTIF_WIN_LIBMRM): $(MOTIF_WIN_BUILD_STAMP) $(MOTIF_WIN_LIBXM)
	@echo "  LD      $@"
	$(Q)$(CC) -shared -o $@ -Wl,--out-implib,$@.a \
	    -Wl,--whole-archive $(MOTIF_WIN_BUILD_DIR)/lib/Mrm/.libs/libMrm.a \
	    -Wl,--no-whole-archive -L$(OUT) -lXm -lXt-compat -lX11-compat \
	    $(WIN_POSIX_LDLIBS)

.PHONY: motif-windows
## Build libXm.dll and libMrm.dll with MinGW-w64 (WINDOWS=1)
motif-windows: $(MOTIF_WIN_LIBXM) $(MOTIF_WIN_LIBMRM)

endif
