/*
 * <setjmp.h> for the Windows (MinGW-w64) build: MinGW's own header plus
 * POSIX sigsetjmp/siglongjmp.
 *
 * They are GCC's __builtin_setjmp/__builtin_longjmp: a plain register
 * restore, with no SEH unwind. That is what the jump out of a SIGSEGV
 * handler needs (compat/win32/signal.c runs the handler on a stack the
 * unwinder could not walk), and signal masks do not exist to be saved. The
 * one restriction: siglongjmp always makes sigsetjmp return 1.
 */
#ifndef LIBX11_COMPAT_WIN32_SETJMP_H
#define LIBX11_COMPAT_WIN32_SETJMP_H

#include_next <setjmp.h>

typedef void *sigjmp_buf[5];
#define sigsetjmp(env, savemask) __builtin_setjmp(env)
#define siglongjmp(env, val) __builtin_longjmp(env, 1)

#endif /* LIBX11_COMPAT_WIN32_SETJMP_H */
