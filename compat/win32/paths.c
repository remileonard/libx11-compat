/* The POSIX path namespace of the Windows build ("/c/Users" <-> "C:\Users");
 * see compat/win32/include/x11compat-win32.h. Each wrapper translates its
 * path arguments to the native form and calls the CRT function it stands
 * in for.
 */
#include <ctype.h>
#include <direct.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wctype.h>
#include <windows.h>

#include "paths-internal.h"

/* x11compat-win32.h gives fopen, stat, ... the assembler names of the
 * wrappers below, so these reach the C runtime's own functions under other
 * C names. */
#define CRT(name) X11COMPAT_ASM_NAME(name)
FILE *crtFopen(const char *path, const char *mode) CRT(fopen);
FILE *crtFreopen(const char *path, const char *mode, FILE *stream) CRT(freopen);
int crtOpen(const char *path, int flags, ...) CRT(_open);
int crtStat(const char *path, struct stat *buf) CRT(stat);
int crtAccess(const char *path, int mode) CRT(_access);
int crtChdir(const char *path) CRT(_chdir);
char *crtGetcwd(char *buf, int size) CRT(_getcwd);
int crtUnlink(const char *path) CRT(_unlink);
int crtRemove(const char *path) CRT(remove);
int crtRename(const char *from, const char *to) CRT(rename);
int crtRmdir(const char *path) CRT(_rmdir);
int crtMkdir(const char *path) CRT(_mkdir);
DIR *crtOpendir(const char *path) CRT(opendir);
struct dirent *crtReaddir(DIR *dir) CRT(readdir);
int crtClosedir(DIR *dir) CRT(closedir);
void crtRewinddir(DIR *dir) CRT(rewinddir);

/* The wrappers, under their own names; the declarations in
 * x11compat-win32.h bind the standard names to these symbols. */
static int isSeparatorW(wchar_t c)
{
    return c == L'/' || c == L'\\';
}

static int isAsciiLetterW(wchar_t c)
{
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

static int isDriveSpecW(const wchar_t *p)
{
    return isAsciiLetterW(p[0]) && p[1] == L':' &&
           (isSeparatorW(p[2]) || p[2] == L'\0');
}

/* x11compatToNativePath for wide paths (std::filesystem, the W APIs). */
const wchar_t *x11compatToNativePathW(const wchar_t *path, wchar_t *out)
{
    size_t len;

    if (!path)
        return NULL;
    for (const wchar_t *p = path + 1; p[-1] && *p; p++) {
        if (isSeparatorW(p[-1]) && isDriveSpecW(p)) {
            path = p;
            break;
        }
    }
    if (path[0] == L'/' && isAsciiLetterW(path[1]) &&
        (isSeparatorW(path[2]) || path[2] == L'\0')) {
        const wchar_t *rest = path + 2;
        while (isSeparatorW(*rest))
            rest++;
        len = wcslen(rest);
        if (len + 4 > PATH_BUFFER_SIZE) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        out[0] = (wchar_t) towupper(path[1]);
        out[1] = L':';
        out[2] = L'/';
        memcpy(out + 3, rest, (len + 1) * sizeof(wchar_t));
        len += 3;
    } else {
        len = wcslen(path);
        if (len + 1 > PATH_BUFFER_SIZE) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        memcpy(out, path, (len + 1) * sizeof(wchar_t));
    }
    while (len > 1 && isSeparatorW(out[len - 1]) &&
           !(len == 3 && out[1] == L':'))
        out[--len] = L'\0';
    return out;
}

int x11compatNativePaths(void)
{
    static int native = -1;
    if (native < 0) {
        const char *mode = getenv("X11COMPAT_PATHS");
        native = mode && strcmp(mode, "native") == 0;
    }
    return native;
}

FILE *x11compatFopen(const char *path, const char *mode);
FILE *x11compatFreopen(const char *path, const char *mode, FILE *stream);
int x11compatOpen(const char *path, int flags, ...);
int x11compatCreat(const char *path, int mode);
int x11compatStat(const char *path, struct stat *buf);
int x11compatAccess(const char *path, int mode);
int x11compatChdir(const char *path);
char *x11compatGetcwd(char *buf, int size);
int x11compatUnlink(const char *path);
int x11compatRemove(const char *path);
int x11compatRename(const char *from, const char *to);
int x11compatRmdir(const char *path);
DIR *x11compatOpendir(const char *path);
struct dirent *x11compatReaddir(DIR *dir);
int x11compatClosedir(DIR *dir);
void x11compatRewinddir(DIR *dir);


static int isSeparator(char c)
{
    return c == '/' || c == '\\';
}

/* "X:" followed by a separator or the end: an absolute drive path. */
static int isDriveSpec(const char *p)
{
    return isalpha((unsigned char) p[0]) && p[1] == ':' &&
           (isSeparator(p[2]) || p[2] == '\0');
}

/* "/x" followed by a separator or the end, but not "//server". */
static int isPosixDrive(const char *p)
{
    return p[0] == '/' && isalpha((unsigned char) p[1]) &&
           (isSeparator(p[2]) || p[2] == '\0');
}

/* Only separators: the root of the namespace, which lists the drives. */
static int isPosixRoot(const char *p)
{
    if (!isSeparator(p[0]))
        return 0;
    while (isSeparator(*p))
        p++;
    return *p == '\0';
}

/* The native form of path in out[PATH_BUFFER_SIZE]; NULL (ENAMETOOLONG)
 * when it does not fit. Paths with no POSIX drive pass through, but for the
 * trailing separator, which the CRT's stat() rejects on directories.
 * Shared with imports.c (paths-internal.h).
 */
const char *x11compatToNativePath(const char *path, char *out)
{
    size_t len;

    if (!path)
        return NULL;
    /* A drive path behind a prefix: "/c/dir/C:\x" or "./C:/x". */
    for (const char *p = path + 1; p[-1] && *p; p++) {
        if (isSeparator(p[-1]) && isDriveSpec(p)) {
            path = p;
            break;
        }
    }
    if (isPosixDrive(path)) {
        const char *rest = path + 2;
        while (isSeparator(*rest))
            rest++;
        len = (size_t) snprintf(out, PATH_BUFFER_SIZE, "%c:/%s",
                                toupper((unsigned char) path[1]), rest);
    } else {
        len = (size_t) snprintf(out, PATH_BUFFER_SIZE, "%s", path);
    }
    if (len >= PATH_BUFFER_SIZE) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    /* Keep "X:/" and "/" themselves: "X:" alone is the drive's current
     * directory. */
    while (len > 1 && isSeparator(out[len - 1]) && !(len == 3 && out[1] == ':'))
        out[--len] = '\0';
    return out;
}

void x11compatPosixifyPath(char *path)
{
    if (!path)
        return;
    if (isDriveSpec(path)) {
        path[1] = (char) tolower((unsigned char) path[0]);
        path[0] = '/';
    }
    for (; *path; path++) {
        if (*path == '\\')
            *path = '/';
    }
}

FILE *x11compatFopen(const char *path, const char *mode)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = x11compatToNativePath(path, buf);
    return native ? crtFopen(native, mode) : NULL;
}

FILE *x11compatFreopen(const char *path, const char *mode, FILE *stream)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = path;
    /* A NULL path changes the stream's mode; pass it through. */
    if (path && !(native = x11compatToNativePath(path, buf)))
        return NULL;
    return crtFreopen(native, mode, stream);
}

int x11compatOpen(const char *path, int flags, ...)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = x11compatToNativePath(path, buf);
    int mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    return native ? crtOpen(native, flags, mode) : -1;
}

int x11compatCreat(const char *path, int mode)
{
    return x11compatOpen(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int x11compatStat(const char *path, struct stat *st)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native;

    if (path && isPosixRoot(path)) {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFDIR | S_IREAD | S_IEXEC;
        st->st_nlink = 1;
        return 0;
    }
    native = x11compatToNativePath(path, buf);
    return native ? crtStat(native, st) : -1;
}

int x11compatAccess(const char *path, int mode)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native;

    if (path && isPosixRoot(path)) {
        if (mode & 2) { /* W_OK */
            errno = EACCES;
            return -1;
        }
        return 0;
    }
    native = x11compatToNativePath(path, buf);
    /* The CRT rejects X_OK (1); a file that exists is "executable". */
    return native ? crtAccess(native, mode & ~1) : -1;
}

int x11compatChdir(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native;

    if (path && isPosixRoot(path)) {
        errno = EACCES;
        return -1;
    }
    native = x11compatToNativePath(path, buf);
    return native ? crtChdir(native) : -1;
}

/* "C:\x\y" -> "/c/x/y", "\\server\share" -> "//server/share"; with
 * X11COMPAT_PATHS=native, "C:/x/y". */
char *x11compatGetcwd(char *buf, int size)
{
    char native[PATH_BUFFER_SIZE];
    size_t len;

    if (!crtGetcwd(native, (int) sizeof(native)))
        return NULL;
    if (x11compatNativePaths()) {
        for (char *p = native; *p; p++) {
            if (*p == '\\')
                *p = '/';
        }
    } else {
        x11compatPosixifyPath(native);
    }
    len = strlen(native);
    if (!buf) {
        /* The POSIX extension: allocate (at least size bytes). */
        size_t alloc =
            size > 0 && (size_t) size > len ? (size_t) size : len + 1;
        buf = malloc(alloc);
        if (!buf) {
            errno = ENOMEM;
            return NULL;
        }
    } else if (size <= 0 || (size_t) size < len + 1) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, native, len + 1);
    return buf;
}

int x11compatUnlink(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = x11compatToNativePath(path, buf);
    return native ? crtUnlink(native) : -1;
}

int x11compatRemove(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = x11compatToNativePath(path, buf);
    return native ? crtRemove(native) : -1;
}

int x11compatRename(const char *from, const char *to)
{
    char fromBuf[PATH_BUFFER_SIZE], toBuf[PATH_BUFFER_SIZE];
    const char *nativeFrom = x11compatToNativePath(from, fromBuf);
    const char *nativeTo = x11compatToNativePath(to, toBuf);
    return nativeFrom && nativeTo ? crtRename(nativeFrom, nativeTo) : -1;
}

int x11compatRmdir(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = x11compatToNativePath(path, buf);
    return native ? crtRmdir(native) : -1;
}

int x11compatMkdir(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = x11compatToNativePath(path, buf);
    return native ? crtMkdir(native) : -1;
}

/* POSIX rename() semantics (replace an existing target); see src/util.h. */
int x11compatRenameReplace(const char *from, const char *to)
{
    char fromBuf[PATH_BUFFER_SIZE], toBuf[PATH_BUFFER_SIZE];
    const char *nativeFrom = x11compatToNativePath(from, fromBuf);
    const char *nativeTo = x11compatToNativePath(to, toBuf);

    if (!nativeFrom || !nativeTo)
        return -1;
    if (MoveFileExA(nativeFrom, nativeTo,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
        return 0;
    errno = EACCES;
    return -1;
}

/*
 * "/" as a directory: one entry per drive letter in use. Its DIR handles are
 * this file's own (RootDir), recognized by address; every other handle is
 * the CRT's. libX11-compat.dll holds the one implementation every module
 * calls, so a handle can be read and closed anywhere.
 */
typedef struct RootDir {
    struct dirent entry;
    DWORD drives;
    int next;
    struct RootDir *link;
} RootDir;

static RootDir *rootDirs;

static RootDir *findRootDir(DIR *dir)
{
    for (RootDir *root = rootDirs; root; root = root->link) {
        if ((DIR *) root == dir)
            return root;
    }
    return NULL;
}

DIR *x11compatOpendir(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native;

    if (path && isPosixRoot(path)) {
        RootDir *root = calloc(1, sizeof(*root));
        if (!root) {
            errno = ENOMEM;
            return NULL;
        }
        root->drives = GetLogicalDrives();
        root->link = rootDirs;
        rootDirs = root;
        return (DIR *) root;
    }
    native = x11compatToNativePath(path, buf);
    return native ? crtOpendir(native) : NULL;
}

struct dirent *x11compatReaddir(DIR *dir)
{
    RootDir *root = findRootDir(dir);

    if (!root)
        return crtReaddir(dir);
    while (root->next < 26) {
        int drive = root->next++;
        if (root->drives & (1u << drive)) {
            root->entry.d_name[0] = (char) ('a' + drive);
            root->entry.d_name[1] = '\0';
            root->entry.d_namlen = 1;
            return &root->entry;
        }
    }
    return NULL;
}

void x11compatRewinddir(DIR *dir)
{
    RootDir *root = findRootDir(dir);

    if (!root) {
        crtRewinddir(dir);
        return;
    }
    root->drives = GetLogicalDrives();
    root->next = 0;
}

int x11compatClosedir(DIR *dir)
{
    RootDir **link;

    for (link = &rootDirs; *link; link = &(*link)->link) {
        if ((DIR *) *link == dir) {
            RootDir *root = *link;
            *link = root->link;
            free(root);
            return 0;
        }
    }
    return crtClosedir(dir);
}
