/*
 * Minimal <pwd.h> for the Windows (MinGW-w64) build: the one ordinary user of
 * compat/win32/include/x11compat-win32.h, built from %USERNAME% and
 * %USERPROFILE% (compat/win32/posix.c). Enough for the toolkits' lookups of
 * the user's name and home directory (resource files, ~ expansion).
 */
#ifndef LIBX11_COMPAT_WIN32_PWD_H
#define LIBX11_COMPAT_WIN32_PWD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct passwd {
    char *pw_name;
    char *pw_passwd;
    uid_t pw_uid;
    gid_t pw_gid;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
};

struct passwd *getpwuid(uid_t uid);
struct passwd *getpwnam(const char *name);
int getpwuid_r(uid_t uid,
               struct passwd *pwd,
               char *buf,
               size_t size,
               struct passwd **result);
int getpwnam_r(const char *name,
               struct passwd *pwd,
               char *buf,
               size_t size,
               struct passwd **result);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_PWD_H */
