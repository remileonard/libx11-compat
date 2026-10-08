/*
 * <signal.h> for the Windows (MinGW-w64) build: MinGW's own header plus the
 * POSIX sigaction() surface it lacks (compat/win32/signal.c).
 *
 * Only SIGSEGV is real beyond what the C runtime's signal() offers: a
 * SIGSEGV handler installed with sigaction() runs when the process takes an
 * access violation, so the POSIX "probe memory under a SIGSEGV handler and
 * siglongjmp out" idiom works (Motif's XmStringIsValid uses it to tell an
 * XmString from a plain char *). SIGBUS has no Windows counterpart: it gets
 * its own number (code saves and restores both handlers, so it must not
 * alias SIGSEGV) and its handler is kept but never runs. Masks are accepted
 * and ignored.
 */
#ifndef LIBX11_COMPAT_WIN32_SIGNAL_H
#define LIBX11_COMPAT_WIN32_SIGNAL_H

#include_next <signal.h>

#ifndef SIGBUS
#define SIGBUS 10
#endif

/* POSIX signals Windows never raises, numbered as on the BSDs, so code that
 * installs handlers for them compiles; signal()/sigaction() report them as
 * unsupported (SIG_ERR / -1). */
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif
#ifndef SIGCHLD
#define SIGCHLD 20
#endif
#ifndef SIGUSR1
#define SIGUSR1 30
#endif
#ifndef SIGUSR2
#define SIGUSR2 31
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long sigset_t;

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

#define SA_RESTART 0x10000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000

static inline int sigemptyset(sigset_t *set)
{
    *set = 0;
    return 0;
}
static inline int sigfillset(sigset_t *set)
{
    *set = ~0UL;
    return 0;
}
static inline int sigaddset(sigset_t *set, int sig)
{
    *set |= 1UL << (sig & 31);
    return 0;
}
static inline int sigdelset(sigset_t *set, int sig)
{
    *set &= ~(1UL << (sig & 31));
    return 0;
}
static inline int sigismember(const sigset_t *set, int sig)
{
    return (*set >> (sig & 31)) & 1;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *old);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_SIGNAL_H */
