# Direct desktop-GL path (no gl4es): tests/test-glx-direct links libx11-compat
# for Xlib + glX* and GLVND's gl-only libOpenGL for gl*, so its calls dispatch
# straight to the system Mesa desktop context src/glx.c creates on EGL. Linking
# libOpenGL rather than libGL.so matters: libGL also exports glX* (via libGLX),
# which would shadow ours and send the client to a real X server. The check
# asserts the binary carries neither libGL.so.1 nor libGLX, runs the GL 1.x
# self-checks (GL_SELECT, GL_FEEDBACK, display lists, attrib stack), then takes
# a headless snapshot and asserts it paints. Linux + GLX=1 + Mesa only; a no-op
# elsewhere, so it is safe inside check-glx.

GLX_DIRECT_BIN := $(OUT)/tests/test-glx-direct

.PHONY: check-glx-direct

ifeq ($(GLX)/$(UNAME_S),1/Linux)
$(GLX_DIRECT_BIN): tests/test-glx-direct.c $(TARGET) $(UPSTREAM_HEADERS_STAMP) \
    $(GL_HDR_CACHE)/GL/gl.h $(GL_HDR_CACHE)/GL/glext.h
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(Q)$(CC) $(CPPFLAGS) $(FP_CFLAGS) $(CFLAGS_EXTRA) -I$(GL_HDR_CACHE) $< \
	    $(TARGET) -lOpenGL $(TEST_LDFLAGS) -o $@

## Render through the direct desktop-GL path (libOpenGL + system Mesa, no gl4es)
check-glx-direct:
	$(Q)egl=$$($(CC) -print-file-name=libEGL.so.1); \
	    ogl=$$($(CC) -print-file-name=libOpenGL.so); \
	    if [ ! -f "$$egl" ] || [ ! -f "$$ogl" ]; then \
	        echo "  SKIP    glx-direct (no system libEGL.so.1/libOpenGL.so; install libegl-dev libopengl-dev)"; \
	        exit 0; \
	    fi; \
	    set -e; \
	    $(MAKE) --no-print-directory $(GLX_DIRECT_BIN); \
	    echo "  CHECK   glx-direct links libOpenGL only"; \
	    needed=$$(readelf -d $(GLX_DIRECT_BIN) | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); \
	    echo "$$needed" | grep -qx 'libOpenGL.so.0' || \
	        { echo "  FAIL    glx-direct does not link libOpenGL.so.0" >&2; exit 1; }; \
	    if echo "$$needed" | grep -Eq '^lib(GL|GLX)\.so'; then \
	        echo "  FAIL    glx-direct links libGL/libGLX: $$needed" >&2; exit 1; \
	    fi; \
	    echo "  CHECK   glx-direct GL 1.x self-checks"; \
	    runner=; [ -n "$${DISPLAY:-}" ] || runner="xvfb-run -a"; \
	    LD_LIBRARY_PATH=$(abspath $(OUT))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} \
	        LIBX11_COMPAT_EGL="$$egl" EGL_PLATFORM=surfaceless \
	        $$runner $(GLX_DIRECT_BIN) --once; \
	    echo "  CHECK   glx-direct render"; \
	    LIBX11_COMPAT_EGL="$$egl" EGL_PLATFORM=surfaceless \
	        scripts/glx-snapshot.sh $(GLX_DIRECT_BIN) $(OUT)/glx-direct.png; \
	    $(PYTHON) scripts/assert-image-content.py $(OUT)/glx-direct.png 0.05
else
check-glx-direct:
	@echo "  SKIP    glx-direct (needs Linux + GLX=1)"
endif
