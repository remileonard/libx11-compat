/*
 * EGL emulation over WGL: the Windows provider behind src/egl-wrapper.c.
 *
 * src/glx.c speaks EGL. Windows has no libEGL but always has opengl32.dll,
 * whose legacy contexts are desktop GL compatibility contexts: the GL 1.x
 * surface (immediate mode, display lists, GL_SELECT, feedback) that old GLX
 * clients such as Open Inventor rely on. This file implements the EGL entry
 * points glx.c uses on top of WGL:
 *
 *   EGLDisplay  one process-wide display (a sentinel; WGL has no display)
 *   EGLConfig   a small fixed table (RGBA/RGB, 24-bit depth, 8-bit stencil),
 *               each bound to a WGL pixel format that can draw to both a
 *               window and a pbuffer
 *   EGLContext  an HGLRC created on its config's hidden window
 *   EGLSurface  a WGL_ARB_pbuffer, double-buffered
 *
 * WGL only makes a context current on a device context with the pixel format
 * it was created for, and a window's pixel format is set once. So every config
 * owns a hidden window carrying its format: its contexts are created there,
 * it stands in for EGL_NO_SURFACE (surfaceless make-current), and its
 * pbuffers share the format.
 *
 * Window surfaces are not provided: glx.c then renders every GLX window into
 * a pbuffer and composites the read-back frame into the X window, the same
 * offscreen path headless Linux uses. The pbuffers are double-buffered so a
 * client's glDrawBuffer(GL_BACK) is valid; glx.c reads the back buffer
 * before any swap, so no SwapBuffers is needed.
 *
 * opengl32.dll is loaded at run time, so libX11-compat.dll does not import it
 * and GLX simply reports absent where it fails to load.
 */
#include "egl-types.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* WGL_ARB_pixel_format / WGL_ARB_pbuffer tokens (wglext.h is not needed for
 * this handful).
 */
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_DRAW_TO_PBUFFER_ARB 0x202D
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_TYPE_RGBA_ARB 0x202B
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_RED_BITS_ARB 0x2015
#define WGL_GREEN_BITS_ARB 0x2017
#define WGL_BLUE_BITS_ARB 0x2019
#define WGL_ALPHA_BITS_ARB 0x201B
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_STENCIL_BITS_ARB 0x2023
#define WGL_ACCELERATION_ARB 0x2003
#define WGL_FULL_ACCELERATION_ARB 0x2027
#define WGL_PBUFFER_WIDTH_ARB 0x2034
#define WGL_PBUFFER_HEIGHT_ARB 0x2035

#define EGL_SUCCESS 0x3000
#define EGL_BAD_ACCESS 0x3002
#define EGL_BAD_ALLOC 0x3003
#define EGL_BAD_ATTRIBUTE 0x3004
#define EGL_BAD_CONFIG 0x3005
#define EGL_BAD_CONTEXT 0x3006
#define EGL_BAD_MATCH 0x3009
#define EGL_BAD_PARAMETER 0x300C
#define EGL_BAD_SURFACE 0x300D
#define EGL_NATIVE_RENDERABLE 0x302D
#define EGL_LEVEL 0x3029
#define EGL_BUFFER_SIZE 0x3020

DECLARE_HANDLE(HPBUFFERARB);

typedef HGLRC(WINAPI *CreateContextFn)(HDC);
typedef BOOL(WINAPI *DeleteContextFn)(HGLRC);
typedef BOOL(WINAPI *MakeCurrentFn)(HDC, HGLRC);
typedef PROC(WINAPI *GetProcAddressFn)(LPCSTR);
typedef BOOL(WINAPI *ShareListsFn)(HGLRC, HGLRC);
typedef BOOL(WINAPI *ChoosePixelFormatArbFn)(HDC,
                                             const int *,
                                             const FLOAT *,
                                             UINT,
                                             int *,
                                             UINT *);
typedef HPBUFFERARB(WINAPI *CreatePbufferFn)(HDC, int, int, int, const int *);
typedef HDC(WINAPI *GetPbufferDcFn)(HPBUFFERARB);
typedef int(WINAPI *ReleasePbufferDcFn)(HPBUFFERARB, HDC);
typedef BOOL(WINAPI *DestroyPbufferFn)(HPBUFFERARB);
typedef BOOL(WINAPI *SwapIntervalFn)(int);

static struct {
    HMODULE opengl32;
    CreateContextFn createContext;
    DeleteContextFn deleteContext;
    MakeCurrentFn makeCurrent;
    GetProcAddressFn getProcAddress;
    ShareListsFn shareLists;
    ChoosePixelFormatArbFn choosePixelFormat;
    CreatePbufferFn createPbuffer;
    GetPbufferDcFn getPbufferDc;
    ReleasePbufferDcFn releasePbufferDc;
    DestroyPbufferFn destroyPbuffer;
    SwapIntervalFn swapInterval;
} wgl;

typedef struct {
    EGLint id;
    EGLint red, green, blue, alpha, depth, stencil;
    int pixelFormat; /* 0 when the driver offered none */
    HWND window;     /* hidden, carries pixelFormat */
    HDC dc;
} WglConfig;

static WglConfig configs[] = {
    {1, 8, 8, 8, 8, 24, 8, 0, NULL, NULL},
    {2, 8, 8, 8, 0, 24, 8, 0, NULL, NULL},
};
#define CONFIG_COUNT ((int) (sizeof(configs) / sizeof(configs[0])))

typedef struct {
    HGLRC rc;
    WglConfig *config;
} WglContext;

typedef struct {
    HPBUFFERARB pbuffer;
    HDC dc;
    EGLint width, height;
    WglConfig *config;
} WglSurface;

#define DISPLAY_SENTINEL ((EGLDisplay) (uintptr_t) 0x1)

static DWORD errorSlot = TLS_OUT_OF_INDEXES;
static int initialized;

static void setError(EGLint error)
{
    if (errorSlot != TLS_OUT_OF_INDEXES)
        TlsSetValue(errorSlot, (LPVOID) (uintptr_t) error);
}

static EGLint wglEglGetError(void)
{
    EGLint error = EGL_SUCCESS;
    if (errorSlot != TLS_OUT_OF_INDEXES) {
        error = (EGLint) (uintptr_t) TlsGetValue(errorSlot);
        TlsSetValue(errorSlot, (LPVOID) (uintptr_t) EGL_SUCCESS);
    }
    return error ? error : EGL_SUCCESS;
}

static const char kWindowClass[] = "libx11-compat-wgl";

static HWND createHiddenWindow(void)
{
    static ATOM atom;
    HINSTANCE instance = GetModuleHandleA(NULL);
    if (!atom) {
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = instance;
        wc.lpszClassName = kWindowClass;
        atom = RegisterClassA(&wc);
        if (!atom)
            return NULL;
    }
    return CreateWindowExA(0, kWindowClass, "", WS_POPUP, 0, 0, 1, 1, NULL,
                           NULL, instance, NULL);
}

static void *glProc(const char *name)
{
    PROC proc = wgl.getProcAddress(name);
    /* wglGetProcAddress answers only for extensions and post-1.1 entry
     * points, and some drivers return small sentinel values on a miss; the
     * GL 1.1 core lives in opengl32.dll's own exports.
     */
    uintptr_t value = (uintptr_t) proc;
    if (value <= 3 || value == (uintptr_t) -1)
        proc = GetProcAddress(wgl.opengl32, name);
    void *result;
    memcpy(&result, &proc, sizeof(result));
    return result;
}

#define LOAD_GL32(field, name) \
    (wgl.field = (void *) GetProcAddress(wgl.opengl32, name)) != NULL

static int loadOpengl32(void)
{
    wgl.opengl32 = LoadLibraryA("opengl32.dll");
    if (!wgl.opengl32)
        return 0;
    if (LOAD_GL32(createContext, "wglCreateContext") &&
        LOAD_GL32(deleteContext, "wglDeleteContext") &&
        LOAD_GL32(makeCurrent, "wglMakeCurrent") &&
        LOAD_GL32(getProcAddress, "wglGetProcAddress") &&
        LOAD_GL32(shareLists, "wglShareLists"))
        return 1;
    FreeLibrary(wgl.opengl32);
    memset(&wgl, 0, sizeof(wgl));
    return 0;
}

/* The ARB pixel-format and pbuffer entry points are only reachable through
 * wglGetProcAddress with a context current, so bootstrap one on a throwaway
 * window with a plain PFD format.
 */
static int loadWglExtensions(void)
{
    HWND window = createHiddenWindow();
    if (!window)
        return 0;
    HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    int ok = 0;
    int format = ChoosePixelFormat(dc, &pfd);
    if (format && SetPixelFormat(dc, format, &pfd)) {
        HGLRC rc = wgl.createContext(dc);
        if (rc && wgl.makeCurrent(dc, rc)) {
            wgl.choosePixelFormat =
                (ChoosePixelFormatArbFn) glProc("wglChoosePixelFormatARB");
            wgl.createPbuffer = (CreatePbufferFn) glProc("wglCreatePbufferARB");
            wgl.getPbufferDc = (GetPbufferDcFn) glProc("wglGetPbufferDCARB");
            wgl.releasePbufferDc =
                (ReleasePbufferDcFn) glProc("wglReleasePbufferDCARB");
            wgl.destroyPbuffer =
                (DestroyPbufferFn) glProc("wglDestroyPbufferARB");
            wgl.swapInterval = (SwapIntervalFn) glProc("wglSwapIntervalEXT");
            ok = wgl.choosePixelFormat && wgl.createPbuffer &&
                 wgl.getPbufferDc && wgl.releasePbufferDc && wgl.destroyPbuffer;
            wgl.makeCurrent(NULL, NULL);
        }
        if (rc)
            wgl.deleteContext(rc);
    }
    ReleaseDC(window, dc);
    DestroyWindow(window);
    return ok;
}

/* Give each config a pixel format that draws to windows and pbuffers, set on
 * the config's own hidden window. A config the driver cannot satisfy is left
 * with pixelFormat 0 and never matches.
 */
static int setUpConfigs(void)
{
    int usable = 0;
    for (int i = 0; i < CONFIG_COUNT; i++) {
        WglConfig *c = &configs[i];
        const int attribs[] = {WGL_DRAW_TO_WINDOW_ARB,
                               TRUE,
                               WGL_DRAW_TO_PBUFFER_ARB,
                               TRUE,
                               WGL_SUPPORT_OPENGL_ARB,
                               TRUE,
                               WGL_DOUBLE_BUFFER_ARB,
                               TRUE,
                               WGL_PIXEL_TYPE_ARB,
                               WGL_TYPE_RGBA_ARB,
                               WGL_RED_BITS_ARB,
                               c->red,
                               WGL_GREEN_BITS_ARB,
                               c->green,
                               WGL_BLUE_BITS_ARB,
                               c->blue,
                               WGL_ALPHA_BITS_ARB,
                               c->alpha,
                               WGL_DEPTH_BITS_ARB,
                               c->depth,
                               WGL_STENCIL_BITS_ARB,
                               c->stencil,
                               0};
        c->window = createHiddenWindow();
        if (!c->window)
            continue;
        c->dc = GetDC(c->window);
        int format = 0;
        UINT count = 0;
        PIXELFORMATDESCRIPTOR pfd;
        if (wgl.choosePixelFormat(c->dc, attribs, NULL, 1, &format, &count) &&
            count > 0 &&
            DescribePixelFormat(c->dc, format, sizeof(pfd), &pfd) &&
            SetPixelFormat(c->dc, format, &pfd)) {
            c->pixelFormat = format;
            usable++;
        } else {
            ReleaseDC(c->window, c->dc);
            DestroyWindow(c->window);
            c->window = NULL;
            c->dc = NULL;
        }
    }
    return usable;
}

static WglConfig *lookUpConfig(EGLConfig config)
{
    for (int i = 0; i < CONFIG_COUNT; i++)
        if ((EGLConfig) &configs[i] == config && configs[i].pixelFormat)
            return &configs[i];
    return NULL;
}

static EGLDisplay wglEglGetDisplay(EGLNativeDisplayType native)
{
    (void) native;
    return DISPLAY_SENTINEL;
}

static EGLBoolean wglEglInitialize(EGLDisplay display,
                                   EGLint *major,
                                   EGLint *minor)
{
    if (display != DISPLAY_SENTINEL) {
        setError(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    if (!initialized) {
        if (!loadWglExtensions() || !setUpConfigs())
            return EGL_FALSE;
        initialized = 1;
    }
    if (major)
        *major = 1;
    if (minor)
        *minor = 4;
    return EGL_TRUE;
}

static EGLBoolean wglEglTerminate(EGLDisplay display)
{
    (void) display;
    return EGL_TRUE;
}

/* Desktop GL is all WGL offers; accepting the GLES API too keeps glx.c's
 * provider probe (bind desktop, then GLES) on the desktop path.
 */
static EGLBoolean wglEglBindApi(EGLenum api)
{
    return api == EGL_OPENGL_API || api == EGL_OPENGL_ES_API;
}

static const char *wglEglQueryString(EGLDisplay display, EGLint name)
{
    (void) display;
    switch (name) {
    case EGL_VENDOR:
        return "libx11-compat WGL";
    case EGL_VERSION:
        return "1.4 WGL";
    case EGL_CLIENT_APIS:
        return "OpenGL";
    case EGL_EXTENSIONS:
        return "";
    default:
        setError(EGL_BAD_PARAMETER);
        return NULL;
    }
}

static EGLBoolean configAttrib(const WglConfig *c, EGLint attrib, EGLint *value)
{
    switch (attrib) {
    case EGL_CONFIG_ID:
        *value = c->id;
        return EGL_TRUE;
    case EGL_RED_SIZE:
        *value = c->red;
        return EGL_TRUE;
    case EGL_GREEN_SIZE:
        *value = c->green;
        return EGL_TRUE;
    case EGL_BLUE_SIZE:
        *value = c->blue;
        return EGL_TRUE;
    case EGL_ALPHA_SIZE:
        *value = c->alpha;
        return EGL_TRUE;
    case EGL_BUFFER_SIZE:
        *value = c->red + c->green + c->blue + c->alpha;
        return EGL_TRUE;
    case EGL_DEPTH_SIZE:
        *value = c->depth;
        return EGL_TRUE;
    case EGL_STENCIL_SIZE:
        *value = c->stencil;
        return EGL_TRUE;
    case EGL_SAMPLES:
    case EGL_SAMPLE_BUFFERS:
    case EGL_LEVEL:
    case EGL_NATIVE_VISUAL_ID:
        *value = 0;
        return EGL_TRUE;
    case EGL_NATIVE_RENDERABLE:
        *value = EGL_TRUE;
        return EGL_TRUE;
    case EGL_SURFACE_TYPE:
        *value = EGL_PBUFFER_BIT | EGL_WINDOW_BIT;
        return EGL_TRUE;
    case EGL_RENDERABLE_TYPE:
        *value = EGL_OPENGL_BIT | EGL_OPENGL_ES2_BIT;
        return EGL_TRUE;
    default:
        return EGL_FALSE;
    }
}

static EGLBoolean wglEglGetConfigAttrib(EGLDisplay display,
                                        EGLConfig config,
                                        EGLint attrib,
                                        EGLint *value)
{
    (void) display;
    WglConfig *c = lookUpConfig(config);
    if (!c) {
        setError(EGL_BAD_CONFIG);
        return EGL_FALSE;
    }
    if (!configAttrib(c, attrib, value)) {
        setError(EGL_BAD_ATTRIBUTE);
        return EGL_FALSE;
    }
    return EGL_TRUE;
}

/* Does config c satisfy one requested attribute? Sizes are minimums, the
 * bitmask attributes must be covered, and anything else this table does not
 * model is accepted.
 */
static int configMatches(const WglConfig *c, EGLint attrib, EGLint wanted)
{
    EGLint have = 0;
    if (!configAttrib(c, attrib, &have))
        return 1;
    switch (attrib) {
    case EGL_SURFACE_TYPE:
    case EGL_RENDERABLE_TYPE:
        return (have & wanted) == wanted;
    case EGL_CONFIG_ID:
    case EGL_LEVEL:
    case EGL_NATIVE_VISUAL_ID:
        return wanted == (EGLint) -1 /* EGL_DONT_CARE */ || have == wanted;
    default:
        return wanted == (EGLint) -1 || have >= wanted;
    }
}

static EGLBoolean wglEglChooseConfig(EGLDisplay display,
                                     const EGLint *attribs,
                                     EGLConfig *out,
                                     EGLint capacity,
                                     EGLint *count)
{
    (void) display;
    EGLint found = 0;
    for (int i = 0; i < CONFIG_COUNT; i++) {
        WglConfig *c = &configs[i];
        if (!c->pixelFormat)
            continue;
        int ok = 1;
        for (const EGLint *a = attribs; ok && a && a[0] != EGL_NONE; a += 2)
            ok = configMatches(c, a[0], a[1]);
        if (!ok)
            continue;
        if (out && found < capacity)
            out[found] = (EGLConfig) c;
        found++;
    }
    if (count)
        *count = out ? (found < capacity ? found : capacity) : found;
    return EGL_TRUE;
}

static EGLContext wglEglCreateContext(EGLDisplay display,
                                      EGLConfig config,
                                      EGLContext share,
                                      const EGLint *attribs)
{
    (void) display;
    (void) attribs; /* a legacy WGL context is the full compatibility GL */
    WglConfig *c = lookUpConfig(config);
    if (!c) {
        setError(EGL_BAD_CONFIG);
        return EGL_NO_CONTEXT;
    }
    WglContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        setError(EGL_BAD_ALLOC);
        return EGL_NO_CONTEXT;
    }
    ctx->config = c;
    ctx->rc = wgl.createContext(c->dc);
    if (!ctx->rc) {
        free(ctx);
        setError(EGL_BAD_ALLOC);
        return EGL_NO_CONTEXT;
    }
    if (share != EGL_NO_CONTEXT &&
        !wgl.shareLists(((WglContext *) share)->rc, ctx->rc)) {
        wgl.deleteContext(ctx->rc);
        free(ctx);
        setError(EGL_BAD_MATCH);
        return EGL_NO_CONTEXT;
    }
    return (EGLContext) ctx;
}

static EGLBoolean wglEglDestroyContext(EGLDisplay display, EGLContext context)
{
    (void) display;
    WglContext *ctx = context;
    if (!ctx) {
        setError(EGL_BAD_CONTEXT);
        return EGL_FALSE;
    }
    wgl.deleteContext(ctx->rc);
    free(ctx);
    return EGL_TRUE;
}

static EGLSurface wglEglCreateWindowSurface(EGLDisplay display,
                                            EGLConfig config,
                                            EGLNativeWindowType window,
                                            const EGLint *attribs)
{
    (void) display;
    (void) config;
    (void) window;
    (void) attribs;
    setError(EGL_BAD_MATCH);
    return EGL_NO_SURFACE;
}

static EGLSurface wglEglCreatePbufferSurface(EGLDisplay display,
                                             EGLConfig config,
                                             const EGLint *attribs)
{
    (void) display;
    WglConfig *c = lookUpConfig(config);
    if (!c) {
        setError(EGL_BAD_CONFIG);
        return EGL_NO_SURFACE;
    }
    EGLint width = 1, height = 1;
    for (const EGLint *a = attribs; a && a[0] != EGL_NONE; a += 2) {
        if (a[0] == EGL_WIDTH)
            width = a[1] > 0 ? a[1] : 1;
        else if (a[0] == EGL_HEIGHT)
            height = a[1] > 0 ? a[1] : 1;
    }
    WglSurface *surface = calloc(1, sizeof(*surface));
    if (!surface) {
        setError(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }
    const int none[] = {0};
    surface->pbuffer =
        wgl.createPbuffer(c->dc, c->pixelFormat, width, height, none);
    surface->dc = surface->pbuffer ? wgl.getPbufferDc(surface->pbuffer) : NULL;
    if (!surface->dc) {
        if (surface->pbuffer)
            wgl.destroyPbuffer(surface->pbuffer);
        free(surface);
        setError(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }
    surface->width = width;
    surface->height = height;
    surface->config = c;
    return (EGLSurface) surface;
}

static EGLBoolean wglEglDestroySurface(EGLDisplay display, EGLSurface surface)
{
    (void) display;
    WglSurface *s = surface;
    if (!s) {
        setError(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    wgl.releasePbufferDc(s->pbuffer, s->dc);
    wgl.destroyPbuffer(s->pbuffer);
    free(s);
    return EGL_TRUE;
}

static EGLBoolean wglEglQuerySurface(EGLDisplay display,
                                     EGLSurface surface,
                                     EGLint attrib,
                                     EGLint *value)
{
    (void) display;
    WglSurface *s = surface;
    if (!s) {
        setError(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    if (attrib == EGL_WIDTH)
        *value = s->width;
    else if (attrib == EGL_HEIGHT)
        *value = s->height;
    else if (attrib == EGL_CONFIG_ID)
        *value = s->config->id;
    else {
        setError(EGL_BAD_ATTRIBUTE);
        return EGL_FALSE;
    }
    return EGL_TRUE;
}

/* WGL binds one device context for both drawing and reading; glx.c only
 * passes distinct draw and read surfaces for glXMakeContextCurrent, which
 * this backend serves with the draw surface.
 */
static EGLBoolean wglEglMakeCurrent(EGLDisplay display,
                                    EGLSurface draw,
                                    EGLSurface read,
                                    EGLContext context)
{
    (void) display;
    (void) read;
    WglContext *ctx = context;
    if (!ctx)
        return wgl.makeCurrent(NULL, NULL) ? EGL_TRUE : EGL_FALSE;
    HDC dc = draw ? ((WglSurface *) draw)->dc : ctx->config->dc;
    if (!wgl.makeCurrent(dc, ctx->rc)) {
        setError(EGL_BAD_ACCESS);
        return EGL_FALSE;
    }
    return EGL_TRUE;
}

static EGLBoolean wglEglSwapBuffers(EGLDisplay display, EGLSurface surface)
{
    (void) display;
    WglSurface *s = surface;
    if (!s) {
        setError(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    return SwapBuffers(s->dc) ? EGL_TRUE : EGL_FALSE;
}

static EGLBoolean wglEglSwapInterval(EGLDisplay display, EGLint interval)
{
    (void) display;
    return wgl.swapInterval && wgl.swapInterval(interval) ? EGL_TRUE
                                                          : EGL_FALSE;
}

static EGLFuncPointer wglEglGetProcAddress(const char *name)
{
    void *proc = glProc(name);
    EGLFuncPointer fn;
    memcpy(&fn, &proc, sizeof(fn));
    return fn;
}

int eglWglLoad(EglApi *api)
{
    if (errorSlot == TLS_OUT_OF_INDEXES)
        errorSlot = TlsAlloc();
    if (!loadOpengl32())
        return 0;
    api->getDisplay = wglEglGetDisplay;
    api->initialize = wglEglInitialize;
    api->terminate = wglEglTerminate;
    api->bindAPI = wglEglBindApi;
    api->queryString = wglEglQueryString;
    api->chooseConfig = wglEglChooseConfig;
    api->getConfigAttrib = wglEglGetConfigAttrib;
    api->createContext = wglEglCreateContext;
    api->destroyContext = wglEglDestroyContext;
    api->createWindowSurface = wglEglCreateWindowSurface;
    api->createPbufferSurface = wglEglCreatePbufferSurface;
    api->destroySurface = wglEglDestroySurface;
    api->querySurface = wglEglQuerySurface;
    api->makeCurrent = wglEglMakeCurrent;
    api->swapBuffers = wglEglSwapBuffers;
    api->swapInterval = wglEglSwapInterval;
    api->getError = wglEglGetError;
    api->getProcAddress = wglEglGetProcAddress;
    return 1;
}
