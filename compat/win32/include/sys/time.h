/*
 * <sys/time.h> for the Windows (MinGW-w64) build: MinGW's own header, plus
 * fd_set and select() as glibc's <sys/time.h> exposes them (through
 * <sys/select.h>). Code
 * such as Open Inventor's SoDB names fd_set in its select()-style hooks after
 * including only <sys/time.h>.
 *
 * fd_set is Winsock's, from the same small MinGW header <winsock2.h> uses
 * (whose include guard keeps a later <winsock2.h> consistent), but without
 * <winsock2.h> itself, which drags in <windows.h> and its macros (ERROR,
 * Arc, BOOL, ...) that collide with X and application names.
 */
#ifndef LIBX11_COMPAT_WIN32_SYS_TIME_H
#define LIBX11_COMPAT_WIN32_SYS_TIME_H

#include_next <sys/time.h>

#include <_bsd_types.h>

/* SOCKET's base type, as <basetsd.h> defines it. That header itself would
 * also bring INT32 and friends, which collide with <X11/Xmd.h>. */
#ifndef _BASETSD_H_
#ifdef _WIN64
__extension__ typedef unsigned long long UINT_PTR;
#else
typedef unsigned int UINT_PTR;
#endif
#endif

#ifndef WINAPI
#define WINAPI __stdcall
#define LIBX11_COMPAT_WINAPI_DEFINED_HERE
#endif
#include <psdk_inc/_fd_types.h>
/* select() as <winsock2.h> declares it (sockets only, like fd_set). */
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) int WINAPI select(int nfds,
                                        fd_set *readfds,
                                        fd_set *writefds,
                                        fd_set *exceptfds,
                                        const struct timeval *timeout);
#ifdef __cplusplus
}
#endif

#ifdef LIBX11_COMPAT_WINAPI_DEFINED_HERE
#undef WINAPI
#undef LIBX11_COMPAT_WINAPI_DEFINED_HERE
#endif

#endif /* LIBX11_COMPAT_WIN32_SYS_TIME_H */
