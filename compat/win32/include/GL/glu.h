/*
 * <GL/glu.h> for the Windows (MinGW-w64) build: the real header, plus, on
 * 32-bit Windows, cdecl GLU callbacks.
 *
 * glu32.dll calls its tessellator, NURBS and quadric callbacks with __stdcall
 * (_GLUfuncptr), the convention of every Win32 GL entry point. Code written
 * for UNIX registers plain C functions, which are cdecl: g++ rejects the
 * conversion, and forcing it would leave the stack unbalanced after every
 * callback. Such code often also registers GL entry points themselves
 * (gluTessCallback(tess, GLU_BEGIN, (void (*)()) glBegin)), which are already
 * __stdcall.
 *
 * So the three registration calls are wrapped: a function that lives in
 * opengl32.dll or glu32.dll is passed through, and any other one gets a small
 * __stdcall thunk that calls it as cdecl (compat/win32/glu-callbacks.c). The
 * thunk's argument count comes from the callback selector. 64-bit Windows has
 * a single calling convention, so there the header is just the real one.
 *
 * With <GL/gl.h> making gl* cdecl on Win32 (see that header), a GL entry
 * point registered as a callback is one of the cdecl wrappers, and gets a
 * thunk like any other function; the opengl32/glu32 pass-through covers
 * code that reaches the native entry points.
 */
#ifndef LIBX11_COMPAT_WIN32_GL_GLU_H
#define LIBX11_COMPAT_WIN32_GL_GLU_H

/* gl.h first, through the wrapper next to this file (cdecl gl* on Win32).
 * GLU itself is glu32.dll's: its entry points and _GLUfuncptr stay __stdcall
 * whatever convention gl.h chose. */
#include <GL/gl.h>
#if defined(_WIN32) && defined(__i386__)
#pragma push_macro("GLAPIENTRY")
#undef GLAPIENTRY
#define GLAPIENTRY __stdcall
#include_next <GL/glu.h>
#pragma pop_macro("GLAPIENTRY")
#else
#include_next <GL/glu.h>
#endif

#if defined(_WIN32) && defined(__i386__)

#ifdef __cplusplus
extern "C" {
#endif

/* Returns fn itself when it already is a Win32 GL/GLU entry point, or a
 * __stdcall thunk taking argWords 32-bit arguments that calls fn as cdecl.
 * A negative argWords (an unknown selector) also returns fn unchanged.
 */
_GLUfuncptr x11compatGluCallback(void (*fn)(void), int argWords);

/* Arguments, in 32-bit words, of the callback a selector installs. */
static inline int x11compatGluCallbackArgWords(GLenum which)
{
    switch (which) {
    case GLU_TESS_END: /* also GLU_END */
    case GLU_NURBS_END:
        return 0;
    case GLU_TESS_BEGIN:     /* GLU_BEGIN */
    case GLU_TESS_VERTEX:    /* GLU_VERTEX */
    case GLU_TESS_ERROR:     /* GLU_ERROR, GLU_NURBS_ERROR */
    case GLU_TESS_EDGE_FLAG: /* GLU_EDGE_FLAG */
    case GLU_TESS_END_DATA:
    case GLU_NURBS_BEGIN:
    case GLU_NURBS_VERTEX:
    case GLU_NURBS_NORMAL:
    case GLU_NURBS_COLOR:
    case GLU_NURBS_TEXTURE_COORD:
    case GLU_NURBS_END_DATA:
        return 1;
    case GLU_TESS_BEGIN_DATA:
    case GLU_TESS_VERTEX_DATA:
    case GLU_TESS_ERROR_DATA:
    case GLU_TESS_EDGE_FLAG_DATA:
    case GLU_NURBS_BEGIN_DATA:
    case GLU_NURBS_VERTEX_DATA:
    case GLU_NURBS_NORMAL_DATA:
    case GLU_NURBS_COLOR_DATA:
    case GLU_NURBS_TEXTURE_COORD_DATA:
        return 2;
    case GLU_TESS_COMBINE:
        return 4;
    case GLU_TESS_COMBINE_DATA:
        return 5;
    default:
        return -1;
    }
}

#ifdef __cplusplus
}
#endif

#define gluTessCallback(tess, which, fn)                       \
    gluTessCallback((tess), (which),                           \
                    x11compatGluCallback((void (*)(void))(fn), \
                                         x11compatGluCallbackArgWords(which)))
#define gluNurbsCallback(nurb, which, fn)          \
    gluNurbsCallback(                              \
        (nurb), (which),                           \
        x11compatGluCallback((void (*)(void))(fn), \
                             x11compatGluCallbackArgWords(which)))
#define gluQuadricCallback(quad, which, fn)        \
    gluQuadricCallback(                            \
        (quad), (which),                           \
        x11compatGluCallback((void (*)(void))(fn), \
                             x11compatGluCallbackArgWords(which)))

#endif /* _WIN32 && __i386__ */

#endif /* LIBX11_COMPAT_WIN32_GL_GLU_H */
