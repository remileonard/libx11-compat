/* POSIX libc calls MinGW-w64 lacks; see compat/win32/include/x11compat-win32.h.
 */
#include <errno.h>
#include <stdlib.h>
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
