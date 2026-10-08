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
 * The system headers come first so their prototypes keep their names, and
 * libstdc++'s <cstdio>, which #undefs fopen and friends, cannot drop the
 * macros later. open, remove and rename are C only: C++ code names methods
 * after them (SoGLCacheList::open, list.remove()).
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

#ifdef __cplusplus
extern "C" {
#endif

FILE *x11compatFopen(const char *path, const char *mode);
FILE *x11compatFreopen(const char *path, const char *mode, FILE *stream);
int x11compatOpen(const char *path, int flags, ...);
int x11compatStat(const char *path, struct stat *buf);
int x11compatAccess(const char *path, int mode);
int x11compatChdir(const char *path);
char *x11compatGetcwd(char *buf, size_t size);
int x11compatUnlink(const char *path);
int x11compatRemove(const char *path);
int x11compatRename(const char *from, const char *to);
int x11compatRmdir(const char *path);
DIR *x11compatOpendir(const char *path);
struct dirent *x11compatReaddir(DIR *dir);
int x11compatClosedir(DIR *dir);
void x11compatRewinddir(DIR *dir);
/* Native path -> POSIX form, in place (same length): "C:\x" -> "/c/x".
 * For Motif, which takes paths typed by the user; NULL is a no-op. */
void x11compatPosixifyPath(char *path);

#ifdef __cplusplus
}
namespace std {
using ::x11compatFopen;
using ::x11compatFreopen;
} // namespace std
#endif

#define fopen(path, mode) x11compatFopen(path, mode)
#define freopen(path, mode, stream) x11compatFreopen(path, mode, stream)
#define stat(path, buf) x11compatStat(path, buf)
#define access(path, mode) x11compatAccess(path, mode)
#define chdir(path) x11compatChdir(path)
#define getcwd(buf, size) x11compatGetcwd(buf, size)
#define unlink(path) x11compatUnlink(path)
#define rmdir(path) x11compatRmdir(path)
#define opendir(path) x11compatOpendir(path)
#define readdir(dir) x11compatReaddir(dir)
#define closedir(dir) x11compatClosedir(dir)
#define rewinddir(dir) x11compatRewinddir(dir)
#ifndef __cplusplus
#define open(...) x11compatOpen(__VA_ARGS__)
#define remove(path) x11compatRemove(path)
#define rename(from, to) x11compatRename(from, to)
#endif

#endif /* LIBX11_COMPAT_WIN32_X11COMPAT_H */
