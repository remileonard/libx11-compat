/*
 * EGL types, tokens and the resolved entry-point table shared by the EGL
 * loader (src/egl-wrapper.c) and the Windows WGL backend (src/egl-wgl.c).
 * Kept free of the X headers so a backend that must include <windows.h>
 * (whose BOOL/Status/None collide with Xlib's) can use it.
 */
#ifndef LIBX11_COMPAT_EGL_TYPES_H
#define LIBX11_COMPAT_EGL_TYPES_H

#include <stdint.h>

/* EGL scalar and handle types (ABI matches the Khronos definitions). */
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef int32_t EGLint;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLNativeDisplayType;
typedef void *EGLNativeWindowType;
typedef void (*EGLFuncPointer)(void);

#define EGL_FALSE 0
#define EGL_TRUE 1
#define EGL_NONE 0x3038
#define EGL_NO_DISPLAY ((EGLDisplay) 0)
#define EGL_NO_CONTEXT ((EGLContext) 0)
#define EGL_NO_SURFACE ((EGLSurface) 0)
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType) 0)

/* eglBindAPI arguments. */
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_OPENGL_API 0x30A2

/* eglQueryString names. */
#define EGL_VENDOR 0x3053
#define EGL_VERSION 0x3054
#define EGL_EXTENSIONS 0x3055
#define EGL_CLIENT_APIS 0x308D

/* Config attributes. */
#define EGL_ALPHA_SIZE 0x3021
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_SAMPLES 0x3031
#define EGL_SAMPLE_BUFFERS 0x3032
#define EGL_SURFACE_TYPE 0x3033
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_RENDERABLE_TYPE_VALUE_NONE 0
#define EGL_NATIVE_VISUAL_ID 0x302E
#define EGL_CONFIG_ID 0x3028

/* EGL_SURFACE_TYPE bits. */
#define EGL_PBUFFER_BIT 0x0001
#define EGL_WINDOW_BIT 0x0004

/* EGL_RENDERABLE_TYPE bits. */
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_OPENGL_ES3_BIT 0x00000040
#define EGL_OPENGL_BIT 0x0008

/* Context / surface creation attributes. */
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056

/* Resolved EGL entry points. Names mirror the C API; a NULL member means the
 * symbol was not found (treated as provider-unavailable).
 */
typedef struct EglApi {
    EGLDisplay (*getDisplay)(EGLNativeDisplayType);
    EGLBoolean (*initialize)(EGLDisplay, EGLint *, EGLint *);
    EGLBoolean (*terminate)(EGLDisplay);
    EGLBoolean (*bindAPI)(EGLenum);
    const char *(*queryString)(EGLDisplay, EGLint);
    EGLBoolean (*chooseConfig)(EGLDisplay,
                               const EGLint *,
                               EGLConfig *,
                               EGLint,
                               EGLint *);
    EGLBoolean (*getConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint *);
    EGLContext (*createContext)(EGLDisplay,
                                EGLConfig,
                                EGLContext,
                                const EGLint *);
    EGLBoolean (*destroyContext)(EGLDisplay, EGLContext);
    EGLSurface (*createWindowSurface)(EGLDisplay,
                                      EGLConfig,
                                      EGLNativeWindowType,
                                      const EGLint *);
    EGLSurface (*createPbufferSurface)(EGLDisplay, EGLConfig, const EGLint *);
    EGLBoolean (*destroySurface)(EGLDisplay, EGLSurface);
    EGLBoolean (*querySurface)(EGLDisplay, EGLSurface, EGLint, EGLint *);
    EGLBoolean (*makeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    EGLBoolean (*swapBuffers)(EGLDisplay, EGLSurface);
    EGLBoolean (*swapInterval)(EGLDisplay, EGLint);
    EGLint (*getError)(void);
    EGLFuncPointer (*getProcAddress)(const char *);
} EglApi;

/* Provider client-API capability, decided once at load. */
typedef enum EglClientApi {
    EGL_CLIENT_API_NONE = 0, /* no provider loaded */
    EGL_CLIENT_API_GLES,     /* OpenGL ES only (e.g. ANGLE) */
    EGL_CLIENT_API_DESKTOP   /* desktop GL available too (e.g. Mesa) */
} EglClientApi;

#ifdef _WIN32
/* Windows has no libEGL: src/egl-wgl.c emulates the entry points above over
 * WGL (opengl32.dll). Fills *api and returns nonzero when WGL is usable.
 */
int eglWglLoad(EglApi *api);
#endif

#endif /* LIBX11_COMPAT_EGL_TYPES_H */
