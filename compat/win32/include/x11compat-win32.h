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

/* u_char, u_short, u_int, u_long: glibc's <sys/types.h> has them; MinGW keeps
 * them in a header of their own (shared with <winsock2.h>). */
#include <_bsd_types.h>

/* struct timeval's tv_usec type in POSIX; MinGW's timeval uses long. */
typedef long suseconds_t;

/* No Unix identities on Windows: everyone is one ordinary, non-root user
 * (non-zero, so code that refuses to trust the environment as root does
 * not take that path). <pwd.h> builds that user from the environment. */
typedef int uid_t;
typedef int gid_t;
static inline uid_t getuid(void)
{
    return 1000;
}
static inline uid_t geteuid(void)
{
    return 1000;
}
static inline gid_t getgid(void)
{
    return 1000;
}
static inline gid_t getegid(void)
{
    return 1000;
}

/* ws2_32's, declared exactly as <winsock2.h> does (that header collides
 * with the X headers); WSAStartup has run once a display is open. */
__declspec(dllimport) int __stdcall gethostname(char *name, int namelen);

int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
/* The POSIX 48-bit linear congruential generator (<stdlib.h>). */
double drand48(void);
long lrand48(void);
void srand48(long seed);
/* POSIX rename() semantics (replace an existing target); see src/util.h. */
int x11compatRenameReplace(const char *from, const char *to);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_X11COMPAT_H */
