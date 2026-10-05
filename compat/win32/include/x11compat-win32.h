/*
 * Force-included into every translation unit of the Windows build
 * (mk/config.mk): the few POSIX libc calls MinGW-w64 does not declare, with
 * their implementations in compat/win32/posix.c. Deliberately free of
 * <windows.h>, which collides with the X headers (BOOL, Status, None, ...).
 */
#ifndef LIBX11_COMPAT_WIN32_X11COMPAT_H
#define LIBX11_COMPAT_WIN32_X11COMPAT_H

/* Have MinGW's <time.h> provide the POSIX reentrant time functions
 * (localtime_r, gmtime_r, ...), as any POSIX libc does. */
#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
#define _POSIX_THREAD_SAFE_FUNCTIONS 200112L
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* struct timeval's tv_usec type in POSIX; MinGW's timeval uses long. */
typedef long suseconds_t;

int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
/* POSIX rename() semantics (replace an existing target); see src/util.h. */
int x11compatRenameReplace(const char *from, const char *to);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_X11COMPAT_H */
