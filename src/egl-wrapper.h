/*
 * Minimal EGL loader for the GLX-over-EGL path.
 *
 * libx11-compat dlopens libEGL at runtime by SONAME; the concrete provider is
 * resolved on the host (ANGLE on macOS, Mesa on Linux). We deliberately do NOT
 * depend on a system <EGL/egl.h>: only the handful of EGL types, tokens, and
 * entry points the GLX layer needs are declared here. This keeps the build
 * self-contained and lets a machine with no EGL still compile libx11-compat and
 * simply report GLX absent.
 *
 * All platform-specific library loading lives behind this wrapper (not in
 * src/glx.c): on Windows there is no libEGL, and src/egl-wgl.c fills the same
 * entry-point table from WGL instead.
 */
#ifndef LIBX11_COMPAT_EGL_WRAPPER_H
#define LIBX11_COMPAT_EGL_WRAPPER_H

#include <stdbool.h>

#include "util.h" /* LIBX11_COMPAT_HIDDEN */

#include "egl-types.h"

/* Load libEGL on first call and cache the outcome.
 *
 * Returns true when a provider is loaded (dlopen + entry points resolved). This
 * is the cheap half: it does NOT eglInitialize the display, so a client can
 * probe GLX and choose a visual without paying the provider's initialize cost
 * (on Mesa, llvmpipe + LLVM). Call eglEnsureReady before any actual GL use.
 * Safe to call from multiple threads; the load runs once.
 */
LIBX11_COMPAT_HIDDEN bool eglLoad(void);

/* Ensure the provider's default display is initialized (the expensive half,
 * deferred by eglLoad). Runs once, lands on the first render-path use.
 *
 * Returns true when the display is ready.
 * eglDefaultDisplay/eglProviderClientApi call it for you.
 */
LIBX11_COMPAT_HIDDEN bool eglEnsureReady(void);

/* True once eglLoad has succeeded. Cheap; does not trigger a load. */
LIBX11_COMPAT_HIDDEN bool eglIsAvailable(void);

/* Resolved entry points; valid only when eglIsAvailable() is true. */
LIBX11_COMPAT_HIDDEN const EglApi *eglApi(void);

/* The initialized default EGLDisplay, or EGL_NO_DISPLAY when unavailable. */
LIBX11_COMPAT_HIDDEN EGLDisplay eglDefaultDisplay(void);

/* Provider capability decided at load. EGL_CLIENT_API_NONE when unavailable. */
LIBX11_COMPAT_HIDDEN EglClientApi eglProviderClientApi(void);

#endif /* LIBX11_COMPAT_EGL_WRAPPER_H */
