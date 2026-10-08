/* <dlfcn.h> subset over the Win32 loader; see compat/win32/include/dlfcn.h. */
#include <dlfcn.h>

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <psapi.h>

static char lastError[256];
static int haveError;

static void setError(const char *what, const char *name)
{
    snprintf(lastError, sizeof(lastError), "%s %s: Windows error %lu", what,
             name ? name : "(null)", (unsigned long) GetLastError());
    haveError = 1;
}

void *dlopen(const char *file, int mode)
{
    HMODULE module;
    if (!file)
        return GetModuleHandleA(NULL);
    if (mode & RTLD_NOLOAD) {
        if (!GetModuleHandleExA(0, file, &module))
            module = NULL;
    } else {
        module = LoadLibraryA(file);
    }
    if (!module)
        setError("dlopen", file);
    return (void *) module;
}

/* Global scope: the executable first, then every loaded module, leaving out
 * `skip` (the caller's own module for RTLD_NEXT). */
static FARPROC searchModules(const char *name, HMODULE skip)
{
    HMODULE modules[1024];
    DWORD needed = 0;
    HMODULE exe = GetModuleHandleA(NULL);
    FARPROC proc = exe != skip ? GetProcAddress(exe, name) : NULL;
    if (!proc && EnumProcessModules(GetCurrentProcess(), modules,
                                    sizeof(modules), &needed)) {
        DWORD count = needed / sizeof(HMODULE);
        if (count > sizeof(modules) / sizeof(modules[0]))
            count = sizeof(modules) / sizeof(modules[0]);
        for (DWORD i = 0; i < count && !proc; i++)
            if (modules[i] != skip)
                proc = GetProcAddress(modules[i], name);
    }
    return proc;
}

/* noinline: the return address must be dlsym's caller, not a frame it was
 * inlined into. */
__attribute__((noinline)) void *dlsym(void *handle, const char *name)
{
    FARPROC proc = NULL;
    if (handle == RTLD_DEFAULT) {
        proc = searchModules(name, NULL);
    } else if (handle == RTLD_NEXT) {
        HMODULE caller = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR) __builtin_return_address(0), &caller);
        proc = searchModules(name, caller);
    } else {
        proc = GetProcAddress((HMODULE) handle, name);
    }
    if (!proc)
        setError("dlsym", name);
    /* FARPROC -> void *: the usual dlsym data/function pointer pun. */
    void *result;
    memcpy(&result, &proc, sizeof(result));
    return result;
}

int dlclose(void *handle)
{
    if (handle == RTLD_DEFAULT || handle == RTLD_NEXT ||
        handle == GetModuleHandleA(NULL))
        return 0;
    if (FreeLibrary((HMODULE) handle))
        return 0;
    setError("dlclose", NULL);
    return -1;
}

char *dlerror(void)
{
    if (!haveError)
        return NULL;
    haveError = 0;
    return lastError;
}

int dladdr(const void *addr, Dl_info *info)
{
    /* One path per thread, as glibc keeps dli_fname valid while the module
     * stays loaded; callers use it right away. */
    static __thread char path[MAX_PATH];
    HMODULE module = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR) addr, &module) ||
        !GetModuleFileNameA(module, path, sizeof(path)))
        return 0;
    info->dli_fname = path;
    info->dli_fbase = (void *) module;
    info->dli_sname = NULL;
    info->dli_saddr = NULL;
    return 1;
}
