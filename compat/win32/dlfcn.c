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

void *dlsym(void *handle, const char *name)
{
    FARPROC proc = NULL;
    if (handle == RTLD_DEFAULT) {
        /* Global scope: the executable first, then every loaded module. */
        HMODULE modules[1024];
        DWORD needed = 0;
        proc = GetProcAddress(GetModuleHandleA(NULL), name);
        if (!proc && EnumProcessModules(GetCurrentProcess(), modules,
                                        sizeof(modules), &needed)) {
            DWORD count = needed / sizeof(HMODULE);
            if (count > sizeof(modules) / sizeof(modules[0]))
                count = sizeof(modules) / sizeof(modules[0]);
            for (DWORD i = 0; i < count && !proc; i++)
                proc = GetProcAddress(modules[i], name);
        }
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
    if (handle == RTLD_DEFAULT || handle == GetModuleHandleA(NULL))
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
