/* The POSIX path namespace of the Windows build ("/c/Users" <-> "C:\Users");
 * see compat/win32/include/x11compat-win32.h. Each wrapper translates its
 * path arguments to the native form and calls the CRT function the macros
 * there stand in for.
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
#include <windows.h>

/* This file implements the macros; it calls the real functions. */
#undef fopen
#undef freopen
#undef open
#undef stat
#undef access
#undef chdir
#undef getcwd
#undef unlink
#undef remove
#undef rename
#undef rmdir
#undef opendir
#undef readdir
#undef closedir
#undef rewinddir
#undef mkdir

#define PATH_BUFFER_SIZE 4096

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
 */
static const char *toNative(const char *path, char *out)
{
    size_t len;

    if (!path)
        return NULL;
    /* A drive path behind a prefix: "/c/dir/C:\x" or "./C:/x". */
    for (const char *p = path + 1; *p; p++) {
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
    const char *native = toNative(path, buf);
    return native ? fopen(native, mode) : NULL;
}

FILE *x11compatFreopen(const char *path, const char *mode, FILE *stream)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = path;
    /* A NULL path changes the stream's mode; pass it through. */
    if (path && !(native = toNative(path, buf)))
        return NULL;
    return freopen(native, mode, stream);
}

int x11compatOpen(const char *path, int flags, ...)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = toNative(path, buf);
    int mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    return native ? open(native, flags, mode) : -1;
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
    native = toNative(path, buf);
    return native ? stat(native, st) : -1;
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
    native = toNative(path, buf);
    /* The CRT rejects X_OK (1); a file that exists is "executable". */
    return native ? access(native, mode & ~1) : -1;
}

int x11compatChdir(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native;

    if (path && isPosixRoot(path)) {
        errno = EACCES;
        return -1;
    }
    native = toNative(path, buf);
    return native ? chdir(native) : -1;
}

/* "C:\x\y" -> "/c/x/y", "\\server\share" -> "//server/share". */
char *x11compatGetcwd(char *buf, size_t size)
{
    char native[PATH_BUFFER_SIZE];
    size_t len;

    if (!_getcwd(native, sizeof(native)))
        return NULL;
    x11compatPosixifyPath(native);
    len = strlen(native);
    if (!buf) {
        if (size < len + 1)
            size = len + 1;
        buf = malloc(size);
        if (!buf) {
            errno = ENOMEM;
            return NULL;
        }
    } else if (size < len + 1) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, native, len + 1);
    return buf;
}

int x11compatUnlink(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = toNative(path, buf);
    return native ? unlink(native) : -1;
}

int x11compatRemove(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = toNative(path, buf);
    return native ? remove(native) : -1;
}

int x11compatRename(const char *from, const char *to)
{
    char fromBuf[PATH_BUFFER_SIZE], toBuf[PATH_BUFFER_SIZE];
    const char *nativeFrom = toNative(from, fromBuf);
    const char *nativeTo = toNative(to, toBuf);
    return nativeFrom && nativeTo ? rename(nativeFrom, nativeTo) : -1;
}

int x11compatRmdir(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = toNative(path, buf);
    return native ? rmdir(native) : -1;
}

int x11compatMkdir(const char *path)
{
    char buf[PATH_BUFFER_SIZE];
    const char *native = toNative(path, buf);
    return native ? _mkdir(native) : -1;
}

/* POSIX rename() semantics (replace an existing target); see src/util.h. */
int x11compatRenameReplace(const char *from, const char *to)
{
    char fromBuf[PATH_BUFFER_SIZE], toBuf[PATH_BUFFER_SIZE];
    const char *nativeFrom = toNative(from, fromBuf);
    const char *nativeTo = toNative(to, toBuf);

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
    native = toNative(path, buf);
    return native ? opendir(native) : NULL;
}

struct dirent *x11compatReaddir(DIR *dir)
{
    RootDir *root = findRootDir(dir);

    if (!root)
        return readdir(dir);
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
        rewinddir(dir);
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
    return closedir(dir);
}
