/*
 * Minimal <dlfcn.h> for the Windows (MinGW-w64) build of libx11-compat.
 *
 * Only the subset the library uses: dlopen/dlsym/dlclose/dlerror over
 * LoadLibrary/GetProcAddress/FreeLibrary. RTLD_DEFAULT searches every module
 * already loaded in the process, like the ELF global scope. RTLD_NOLOAD maps
 * to GetModuleHandleEx (no load, but takes a reference, as on glibc).
 * RTLD_NEXT searches the same way but skips the module that holds the
 * caller, so a forwarding shim finds the real definition elsewhere. The
 * other mode flags have no Windows meaning and are accepted and ignored.
 */
#ifndef LIBX11_COMPAT_WIN32_DLFCN_H
#define LIBX11_COMPAT_WIN32_DLFCN_H

#define RTLD_LAZY 0x0001
#define RTLD_NOW 0x0002
#define RTLD_GLOBAL 0x0100
#define RTLD_LOCAL 0x0000
#define RTLD_NOLOAD 0x0004
#define RTLD_DEFAULT ((void *) 0)
#define RTLD_NEXT ((void *) -1)

#ifdef __cplusplus
extern "C" {
#endif

/* dladdr: the module holding an address (dli_fname, dli_fbase); the
 * symbol fields are always NULL, as PE keeps no symbol table at run time.
 */
typedef struct {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info;

void *dlopen(const char *file, int mode);
void *dlsym(void *handle, const char *name);
int dlclose(void *handle);
char *dlerror(void);
int dladdr(const void *addr, Dl_info *info);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_DLFCN_H */
