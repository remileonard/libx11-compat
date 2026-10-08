/* sigaction() over the C runtime's signal(), with SIGSEGV delivered from a
 * vectored exception handler; see compat/win32/include/signal.h. */
#include <signal.h>

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

static void (*volatile segvHandler)(int) = SIG_DFL;
static void (*busHandler)(int) = SIG_DFL;
static PVOID faultHook;

/* Runs in place of the faulting instruction. A POSIX handler either
 * siglongjmps away or returns to retry the access; retrying would only
 * fault again, so a returning handler ends the process like SIG_DFL. */
static void __attribute__((noreturn, used)) faultTrampoline(void)
{
    segvHandler(SIGSEGV);
    abort();
}

static LONG CALLBACK onFault(PEXCEPTION_POINTERS info)
{
    void (*handler)(int) = segvHandler;
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        handler == SIG_DFL || handler == SIG_IGN)
        return EXCEPTION_CONTINUE_SEARCH;

    /* Call faultTrampoline on fresh stack below the faulting frame: aligned
     * as at a function entry (sp % 16 == 16 - pointer size), past the
     * x64 shadow space. */
    CONTEXT *context = info->ContextRecord;
#if defined(__x86_64__)
    context->Rsp = ((context->Rsp & ~(DWORD64) 15) - 64) - 8;
    context->Rip = (DWORD64) (uintptr_t) faultTrampoline;
#elif defined(__i386__)
    context->Esp = ((context->Esp & ~(DWORD) 15) - 64) - 4;
    context->Eip = (DWORD) (uintptr_t) faultTrampoline;
#elif defined(__aarch64__)
    context->Sp = (context->Sp & ~(DWORD64) 15) - 64;
    context->Pc = (DWORD64) (uintptr_t) faultTrampoline;
#else
#error "compat/win32/signal.c: unsupported architecture"
#endif
    return EXCEPTION_CONTINUE_EXECUTION;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{
    if (sig == SIGBUS) {
        if (old) {
            old->sa_handler = busHandler;
            old->sa_mask = 0;
            old->sa_flags = 0;
        }
        if (act)
            busHandler = act->sa_handler;
        return 0;
    }
    if (sig == SIGSEGV) {
        if (old) {
            old->sa_handler = segvHandler;
            old->sa_mask = 0;
            old->sa_flags = 0;
        }
        if (act) {
            if (!faultHook)
                faultHook = AddVectoredExceptionHandler(1, onFault);
            segvHandler = act->sa_handler;
        }
        return 0;
    }

    void (*previous)(int) =
        act ? signal(sig, act->sa_handler) : signal(sig, SIG_DFL);
    if (previous == SIG_ERR)
        return -1;
    if (!act) /* query only: put the handler back */
        signal(sig, previous);
    if (old) {
        old->sa_handler = previous;
        old->sa_mask = 0;
        old->sa_flags = 0;
    }
    return 0;
}
