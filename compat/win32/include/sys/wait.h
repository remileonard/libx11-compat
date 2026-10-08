/*
 * <sys/wait.h> for the Windows (MinGW-w64) build. Windows has no fork(), so
 * fork() and the wait calls declared here always fail with ENOSYS; code that
 * forks a helper (Open Inventor's print dialog runs lp) takes its error path.
 */
#ifndef LIBX11_COMPAT_WIN32_SYS_WAIT_H
#define LIBX11_COMPAT_WIN32_SYS_WAIT_H

#include <errno.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WNOHANG 1
#define WUNTRACED 2

#define WIFEXITED(status) (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WIFSIGNALED(status) \
    (((status) & 0x7f) != 0 && ((status) & 0x7f) != 0x7f)
#define WTERMSIG(status) ((status) & 0x7f)
#define WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
#define WSTOPSIG(status) WEXITSTATUS(status)

static inline pid_t fork(void)
{
    errno = ENOSYS;
    return -1;
}

static inline pid_t waitpid(pid_t pid, int *status, int options)
{
    (void) pid;
    (void) status;
    (void) options;
    errno = ECHILD;
    return -1;
}

static inline pid_t wait(int *status)
{
    return waitpid(-1, status, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_SYS_WAIT_H */
