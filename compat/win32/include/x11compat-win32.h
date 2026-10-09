/*
 * Force-included into every translation unit of the Windows build
 * (mk/config.mk): the few POSIX libc calls MinGW-w64 does not declare, with
 * their implementations in compat/win32/posix.c. Deliberately free of
 * <windows.h>, which collides with the X headers (BOOL, Status, None, ...).
 */
#ifndef LIBX11_COMPAT_WIN32_X11COMPAT_H
#define LIBX11_COMPAT_WIN32_X11COMPAT_H

/* MinGW's fortified headers define open() and friends inline, which the
 * path redirects at the end of this file cannot redeclare. */
#undef _FORTIFY_SOURCE

/* Have MinGW's <time.h> provide the POSIX reentrant time functions
 * (localtime_r, gmtime_r, ...), as any POSIX libc does. */
#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
#define _POSIX_THREAD_SAFE_FUNCTIONS 200112L
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* MinGW's <stdlib.h> does `#define environ _environ`, which rewrites every
 * identifier named environ (Open Inventor has a local `SoEnvironment
 * *environ`). Include it first and drop the macro; its include guard keeps
 * any later <stdlib.h> or <cstdlib> from defining it again. POSIX code
 * declares the environ variable itself, and none of this tree does. */
#include <stdlib.h>
#ifdef environ
#undef environ
#endif

/* u_char, u_short, u_int, u_long: glibc's <sys/types.h> has them; MinGW keeps
 * them in a header of their own (shared with <winsock2.h>). */
#include <_bsd_types.h>
/* and the BSD "core address" type. */
typedef char *caddr_t;

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
/* POSIX mkdir(path, mode); MinGW's <io.h> has mkdir(path). Both forms map
 * to x11compatMkdir, which ignores the mode (Windows has no permission
 * bits). Defined before <io.h>, so its own prototype is renamed too. */
#define mkdir(path, ...) x11compatMkdir(path)
int x11compatMkdir(const char *path);

/* The POSIX 48-bit linear congruential generator (<stdlib.h>). */
double drand48(void);
long lrand48(void);
void srand48(long seed);
/* POSIX rename() semantics (replace an existing target); see src/util.h. */
int x11compatRenameReplace(const char *from, const char *to);

#ifdef __cplusplus
}
#endif

/*
 * POSIX path namespace (compat/win32/paths.c). UNIX code, Motif's
 * XmFileSelectionBox first, assumes an absolute path starts with '/' and uses
 * '/' as the only separator. So the file calls below see Windows drives the
 * way MSYS2 and Cygwin show them:
 *
 *   /c/Users/me/model.iv  <->  C:\Users\me\model.iv
 *   /                     ->   a directory listing the drives (c, d, ...)
 *
 * getcwd() returns that form, and every call taking a path accepts it as
 * well as a native one ("C:\x", "C:/x", relative), so a path can go from
 * Motif to fopen() and back. A native path behind a directory prefix, as
 * Motif or Open Inventor build it ("/c/dir/C:\x\model.iv", "./C:/x"),
 * resolves to the native path: ':' cannot appear in a Windows file name.
 *
 * The functions are redirected at the symbol level, not with macros: each is
 * redeclared below, after its system header, with the assembler name of its
 * wrapper (fopen -> x11compatFopen), so every call, in C and C++ alike, and
 * every function pointer taken to it reaches the wrapper. Members of the same
 * name (SoGLCacheList::open, std::list::remove, struct stat) are untouched,
 * which macros could not guarantee. libX11-compat.dll defines the wrappers
 * and exports them (tests/win32-path-symbols.txt).
 */
#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <direct.h>
#include <dirent.h>
/* io.h maps access() to __mingw_access under __USE_MINGW_ACCESS; the
 * wrapper below handles X_OK itself. */
#undef access

#define X11COMPAT_STRINGIFY2(x) #x
#define X11COMPAT_STRINGIFY(x) X11COMPAT_STRINGIFY2(x)
#define X11COMPAT_ASM_NAME(name)                                              \
    __asm__(X11COMPAT_STRINGIFY(__USER_LABEL_PREFIX__) #name)

#ifdef __cplusplus
extern "C" {
#endif

FILE *__cdecl fopen(const char *__restrict__ path,
                    const char *__restrict__ mode)
    X11COMPAT_ASM_NAME(x11compatFopen);
FILE *__cdecl freopen(const char *__restrict__ path,
                      const char *__restrict__ mode,
                      FILE *__restrict__ stream)
    X11COMPAT_ASM_NAME(x11compatFreopen);
int __cdecl open(const char *path, int flags, ...)
    X11COMPAT_ASM_NAME(x11compatOpen);
int __cdecl creat(const char *path, int mode) X11COMPAT_ASM_NAME(x11compatCreat);
int __cdecl stat(const char *path, struct stat *buf)
    X11COMPAT_ASM_NAME(x11compatStat);
int __cdecl access(const char *path, int mode)
    X11COMPAT_ASM_NAME(x11compatAccess);
int __cdecl chdir(const char *path) X11COMPAT_ASM_NAME(x11compatChdir);
char *__cdecl getcwd(char *buf, int size) X11COMPAT_ASM_NAME(x11compatGetcwd);
int __cdecl unlink(const char *path) X11COMPAT_ASM_NAME(x11compatUnlink);
int __cdecl remove(const char *path) X11COMPAT_ASM_NAME(x11compatRemove);
int __cdecl rename(const char *from, const char *to)
    X11COMPAT_ASM_NAME(x11compatRename);
int __cdecl rmdir(const char *path) X11COMPAT_ASM_NAME(x11compatRmdir);
DIR *__cdecl opendir(const char *path) X11COMPAT_ASM_NAME(x11compatOpendir);
struct dirent *__cdecl readdir(DIR *dir) X11COMPAT_ASM_NAME(x11compatReaddir);
int __cdecl closedir(DIR *dir) X11COMPAT_ASM_NAME(x11compatClosedir);
void __cdecl rewinddir(DIR *dir) X11COMPAT_ASM_NAME(x11compatRewinddir);

/* Native path -> POSIX form, in place (same length): "C:\x" -> "/c/x".
 * For Motif, which takes paths typed by the user; NULL is a no-op. */
void x11compatPosixifyPath(char *path);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_X11COMPAT_H */
