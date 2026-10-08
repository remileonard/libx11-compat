/* POSIX libc calls MinGW-w64 lacks; see compat/win32/include/x11compat-win32.h.
 */
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name))
        return 0;
    return _putenv_s(name, value) == 0 ? 0 : -1;
}

int unsetenv(const char *name)
{
    /* An empty value removes the variable from the CRT environment. */
    return _putenv_s(name, "") == 0 ? 0 : -1;
}

int x11compatRenameReplace(const char *from, const char *to)
{
    if (MoveFileExA(from, to,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
        return 0;
    errno = EACCES;
    return -1;
}

/* The single user behind <pwd.h>: the account name and profile directory. */
static struct passwd *currentUser(void)
{
    static struct passwd user;
    static char name[256], dir[MAX_PATH];
    const char *env;

    env = getenv("USERNAME");
    snprintf(name, sizeof(name), "%s", env ? env : "user");
    env = getenv("HOME");
    if (!env)
        env = getenv("USERPROFILE");
    snprintf(dir, sizeof(dir), "%s", env ? env : "C:\\");
    user.pw_name = name;
    user.pw_passwd = "";
    user.pw_uid = getuid();
    user.pw_gid = getgid();
    user.pw_gecos = name;
    user.pw_dir = dir;
    user.pw_shell = "cmd.exe";
    return &user;
}

struct passwd *getpwuid(uid_t uid)
{
    return uid == getuid() ? currentUser() : NULL;
}

struct passwd *getpwnam(const char *name)
{
    struct passwd *user = currentUser();
    return name && _stricmp(name, user->pw_name) == 0 ? user : NULL;
}

/* The reentrant forms copy the user's strings into the caller's buffer. */
static int copyUser(const struct passwd *from,
                    struct passwd *pwd,
                    char *buf,
                    size_t size,
                    struct passwd **result)
{
    size_t nameLen = strlen(from->pw_name) + 1;
    size_t dirLen = strlen(from->pw_dir) + 1;
    *result = NULL;
    if (nameLen + dirLen > size)
        return ERANGE;
    *pwd = *from;
    pwd->pw_name = memcpy(buf, from->pw_name, nameLen);
    pwd->pw_gecos = pwd->pw_name;
    pwd->pw_dir = memcpy(buf + nameLen, from->pw_dir, dirLen);
    *result = pwd;
    return 0;
}

int getpwuid_r(uid_t uid,
               struct passwd *pwd,
               char *buf,
               size_t size,
               struct passwd **result)
{
    struct passwd *user = getpwuid(uid);
    *result = NULL;
    return user ? copyUser(user, pwd, buf, size, result) : 0;
}

int getpwnam_r(const char *name,
               struct passwd *pwd,
               char *buf,
               size_t size,
               struct passwd **result)
{
    struct passwd *user = getpwnam(name);
    *result = NULL;
    return user ? copyUser(user, pwd, buf, size, result) : 0;
}

/* drand48 family: X(n+1) = (a * X(n) + c) mod 2^48, as POSIX specifies. */
static unsigned long long rand48State = 0x1234ABCD330EULL;

static unsigned long long rand48Next(void)
{
    rand48State = (0x5DEECE66DULL * rand48State + 0xB) & 0xFFFFFFFFFFFFULL;
    return rand48State;
}

double drand48(void)
{
    return (double) rand48Next() / 281474976710656.0; /* 2^48 */
}

long lrand48(void)
{
    return (long) (rand48Next() >> 17); /* the high 31 bits */
}

void srand48(long seed)
{
    rand48State = ((unsigned long long) (unsigned long) seed << 16) | 0x330E;
}

/* Legacy BSD calls, defined but not declared: old code declares them itself
 * (Open Inventor's NURBS code, with an int length), and a second prototype
 * would conflict with that one. */
void bcopy(const void *src, void *dst, size_t n);
void bzero(void *s, size_t n);

void bcopy(const void *src, void *dst, size_t n)
{
    memmove(dst, src, n);
}

void bzero(void *s, size_t n)
{
    memset(s, 0, n);
}
