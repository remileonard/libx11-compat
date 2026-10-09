/*
 * The POSIX path namespace ("/c/Users" <-> "C:\Users", compat/win32/paths.c)
 * for code that was not compiled with x11compat-win32.h: libstdc++-6.dll
 * (std::ifstream and friends open through fopen, std::filesystem through
 * _wfopen, _wstat64, CreateFileW, ...), a libstdc++ linked statically into
 * a program, and any other library handed a path from getcwd() or a Motif
 * file dialog.
 *
 * At load, libX11-compat.dll rewrites the import address tables of the
 * modules already loaded, so their calls to the C runtime's and kernel32's
 * path functions reach the wrappers below, which translate a "/c/..." path
 * and call the original function. Only inputs are translated: these modules
 * were written for Windows, so _getcwd(), _fullpath() or GetFullPathName()
 * still give them native paths. dlopen() patches the modules it loads.
 *
 * Left alone: libX11-compat itself (its own calls are the originals the
 * wrappers end in), the system's DLLs (under the Windows directory), and
 * modules using the Universal CRT (ucrtbase / api-ms-win-crt-*), whose
 * FILE and descriptors are not msvcrt's: a wrapper calling msvcrt would hand
 * them the wrong ones.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <psapi.h>

#include "paths-internal.h"

/* Exported (tests/win32-path-symbols.txt), so dlopen() in the toolkit DLLs'
 * copy of compat/win32/dlfcn.c can find it with GetProcAddress. */
void x11compatPatchImports(void);

/* The original functions, from msvcrt.dll and kernel32.dll. */
typedef FILE *(__cdecl *FopenFn)(const char *, const char *);
typedef FILE *(__cdecl *FreopenFn)(const char *, const char *, FILE *);
typedef int(__cdecl *OpenFn)(const char *, int, ...);
typedef int(__cdecl *PathIntFn)(const char *, int);
typedef int(__cdecl *PathFn)(const char *);
typedef int(__cdecl *PathPathFn)(const char *, const char *);
typedef int(__cdecl *StatFn)(const char *, void *);
typedef char *(__cdecl *FullpathFn)(char *, const char *, size_t);
typedef FILE *(__cdecl *WfopenFn)(const wchar_t *, const wchar_t *);
typedef int(__cdecl *WopenFn)(const wchar_t *, int, ...);
typedef int(__cdecl *WpathIntFn)(const wchar_t *, int);
typedef int(__cdecl *WpathFn)(const wchar_t *);
typedef int(__cdecl *WpathPathFn)(const wchar_t *, const wchar_t *);
typedef int(__cdecl *WstatFn)(const wchar_t *, void *);
typedef wchar_t *(__cdecl *WfullpathFn)(wchar_t *, const wchar_t *, size_t);
typedef HANDLE(WINAPI *CreateFileAFn)(LPCSTR,
                                      DWORD,
                                      DWORD,
                                      LPSECURITY_ATTRIBUTES,
                                      DWORD,
                                      DWORD,
                                      HANDLE);
typedef HANDLE(WINAPI *CreateFileWFn)(LPCWSTR,
                                      DWORD,
                                      DWORD,
                                      LPSECURITY_ATTRIBUTES,
                                      DWORD,
                                      DWORD,
                                      HANDLE);
typedef DWORD(WINAPI *GetAttributesAFn)(LPCSTR);
typedef DWORD(WINAPI *GetAttributesWFn)(LPCWSTR);
typedef BOOL(WINAPI *GetAttributesExAFn)(LPCSTR,
                                         GET_FILEEX_INFO_LEVELS,
                                         LPVOID);
typedef BOOL(WINAPI *GetAttributesExWFn)(LPCWSTR,
                                         GET_FILEEX_INFO_LEVELS,
                                         LPVOID);
typedef BOOL(WINAPI *PathBoolAFn)(LPCSTR);
typedef BOOL(WINAPI *PathBoolWFn)(LPCWSTR);
typedef BOOL(WINAPI *CreateDirectoryAFn)(LPCSTR, LPSECURITY_ATTRIBUTES);
typedef BOOL(WINAPI *CreateDirectoryWFn)(LPCWSTR, LPSECURITY_ATTRIBUTES);
typedef BOOL(WINAPI *MoveFileExAFn)(LPCSTR, LPCSTR, DWORD);
typedef BOOL(WINAPI *MoveFileExWFn)(LPCWSTR, LPCWSTR, DWORD);
typedef DWORD(WINAPI *FullPathNameAFn)(LPCSTR, DWORD, LPSTR, LPSTR *);
typedef DWORD(WINAPI *FullPathNameWFn)(LPCWSTR, DWORD, LPWSTR, LPWSTR *);
typedef HANDLE(WINAPI *FindFirstAFn)(LPCSTR, LPWIN32_FIND_DATAA);
typedef HANDLE(WINAPI *FindFirstWFn)(LPCWSTR, LPWIN32_FIND_DATAW);

static FopenFn realFopen;
static FreopenFn realFreopen;
static OpenFn realOpen;
static PathIntFn realCreat, realAccess, realMkdirMode;
static PathFn realChdir, realUnlink, realRemove, realRmdir, realMkdir;
static PathPathFn realRename;
static StatFn realStat, realStati64, realStat64, realStat32, realStat32i64,
    realStat64i32;
static FullpathFn realFullpath;
static WfopenFn realWfopen;
static WopenFn realWopen;
static WpathIntFn realWcreat, realWaccess;
static WpathFn realWchdir, realWunlink, realWremove, realWrmdir, realWmkdir;
static WpathPathFn realWrename;
static WstatFn realWstat, realWstati64, realWstat64, realWstat32,
    realWstat32i64, realWstat64i32;
static WfullpathFn realWfullpath;
static CreateFileAFn realCreateFileA;
static CreateFileWFn realCreateFileW;
static GetAttributesAFn realGetFileAttributesA;
static GetAttributesWFn realGetFileAttributesW;
static GetAttributesExAFn realGetFileAttributesExA;
static GetAttributesExWFn realGetFileAttributesExW;
static PathBoolAFn realDeleteFileA, realRemoveDirectoryA,
    realSetCurrentDirectoryA;
static PathBoolWFn realDeleteFileW, realRemoveDirectoryW,
    realSetCurrentDirectoryW;
static CreateDirectoryAFn realCreateDirectoryA;
static CreateDirectoryWFn realCreateDirectoryW;
static MoveFileExAFn realMoveFileExA;
static MoveFileExWFn realMoveFileExW;
static FullPathNameAFn realGetFullPathNameA;
static FullPathNameWFn realGetFullPathNameW;
static FindFirstAFn realFindFirstFileA;
static FindFirstWFn realFindFirstFileW;

/* NATIVE(path) declares the native form of a narrow path argument, and
 * returns `fail` when it does not fit; NATIVEW the same for a wide one. */
#define NATIVE(var, path, fail)                              \
    char var##Buf[PATH_BUFFER_SIZE];                         \
    const char *var = x11compatToNativePath(path, var##Buf); \
    if (path && !var)                                        \
    return fail
#define NATIVEW(var, path, fail)                                 \
    wchar_t var##Buf[PATH_BUFFER_SIZE];                          \
    const wchar_t *var = x11compatToNativePathW(path, var##Buf); \
    if (path && !var)                                            \
    return fail

/* The C runtime: narrow. */
static FILE *__cdecl hookFopen(const char *path, const char *mode)
{
    NATIVE(native, path, NULL);
    return realFopen(native, mode);
}

static FILE *__cdecl hookFreopen(const char *path, const char *mode, FILE *f)
{
    NATIVE(native, path, NULL);
    return realFreopen(native, mode, f);
}

static int __cdecl hookOpen(const char *path, int flags, ...)
{
    va_list args;
    int mode;
    NATIVE(native, path, -1);
    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);
    return realOpen(native, flags, mode);
}

#define HOOK_PATH_INT(hook, real)                      \
    static int __cdecl hook(const char *path, int arg) \
    {                                                  \
        NATIVE(native, path, -1);                      \
        return real(native, arg);                      \
    }
#define HOOK_PATH(hook, real)                 \
    static int __cdecl hook(const char *path) \
    {                                         \
        NATIVE(native, path, -1);             \
        return real(native);                  \
    }
#define HOOK_STAT(hook, real)                            \
    static int __cdecl hook(const char *path, void *buf) \
    {                                                    \
        NATIVE(native, path, -1);                        \
        return real(native, buf);                        \
    }
HOOK_PATH_INT(hookCreat, realCreat)
HOOK_PATH_INT(hookAccess, realAccess)
HOOK_PATH(hookChdir, realChdir)
HOOK_PATH(hookUnlink, realUnlink)
HOOK_PATH(hookRemove, realRemove)
HOOK_PATH(hookRmdir, realRmdir)
HOOK_PATH(hookMkdir, realMkdir)
HOOK_STAT(hookStat, realStat)
HOOK_STAT(hookStati64, realStati64)
HOOK_STAT(hookStat64, realStat64)
HOOK_STAT(hookStat32, realStat32)
HOOK_STAT(hookStat32i64, realStat32i64)
HOOK_STAT(hookStat64i32, realStat64i32)

static int __cdecl hookRename(const char *from, const char *to)
{
    NATIVE(nativeFrom, from, -1);
    NATIVE(nativeTo, to, -1);
    return realRename(nativeFrom, nativeTo);
}

static char *__cdecl hookFullpath(char *out, const char *path, size_t size)
{
    NATIVE(native, path, NULL);
    return realFullpath(out, native, size);
}

/* The C runtime: wide. */
static FILE *__cdecl hookWfopen(const wchar_t *path, const wchar_t *mode)
{
    NATIVEW(native, path, NULL);
    return realWfopen(native, mode);
}

static int __cdecl hookWopen(const wchar_t *path, int flags, ...)
{
    va_list args;
    int mode;
    NATIVEW(native, path, -1);
    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);
    return realWopen(native, flags, mode);
}

#define HOOK_WPATH_INT(hook, real)                        \
    static int __cdecl hook(const wchar_t *path, int arg) \
    {                                                     \
        NATIVEW(native, path, -1);                        \
        return real(native, arg);                         \
    }
#define HOOK_WPATH(hook, real)                   \
    static int __cdecl hook(const wchar_t *path) \
    {                                            \
        NATIVEW(native, path, -1);               \
        return real(native);                     \
    }
#define HOOK_WSTAT(hook, real)                              \
    static int __cdecl hook(const wchar_t *path, void *buf) \
    {                                                       \
        NATIVEW(native, path, -1);                          \
        return real(native, buf);                           \
    }
HOOK_WPATH_INT(hookWcreat, realWcreat)
HOOK_WPATH_INT(hookWaccess, realWaccess)
HOOK_WPATH(hookWchdir, realWchdir)
HOOK_WPATH(hookWunlink, realWunlink)
HOOK_WPATH(hookWremove, realWremove)
HOOK_WPATH(hookWrmdir, realWrmdir)
HOOK_WPATH(hookWmkdir, realWmkdir)
HOOK_WSTAT(hookWstat, realWstat)
HOOK_WSTAT(hookWstati64, realWstati64)
HOOK_WSTAT(hookWstat64, realWstat64)
HOOK_WSTAT(hookWstat32, realWstat32)
HOOK_WSTAT(hookWstat32i64, realWstat32i64)
HOOK_WSTAT(hookWstat64i32, realWstat64i32)

static int __cdecl hookWrename(const wchar_t *from, const wchar_t *to)
{
    NATIVEW(nativeFrom, from, -1);
    NATIVEW(nativeTo, to, -1);
    return realWrename(nativeFrom, nativeTo);
}

static wchar_t *__cdecl hookWfullpath(wchar_t *out,
                                      const wchar_t *path,
                                      size_t size)
{
    NATIVEW(native, path, NULL);
    return realWfullpath(out, native, size);
}

/* kernel32. */
static HANDLE WINAPI hookCreateFileA(LPCSTR path,
                                     DWORD access,
                                     DWORD share,
                                     LPSECURITY_ATTRIBUTES security,
                                     DWORD disposition,
                                     DWORD flags,
                                     HANDLE templ)
{
    NATIVE(native, path, INVALID_HANDLE_VALUE);
    return realCreateFileA(native, access, share, security, disposition, flags,
                           templ);
}

static HANDLE WINAPI hookCreateFileW(LPCWSTR path,
                                     DWORD access,
                                     DWORD share,
                                     LPSECURITY_ATTRIBUTES security,
                                     DWORD disposition,
                                     DWORD flags,
                                     HANDLE templ)
{
    NATIVEW(native, path, INVALID_HANDLE_VALUE);
    return realCreateFileW(native, access, share, security, disposition, flags,
                           templ);
}

static DWORD WINAPI hookGetFileAttributesA(LPCSTR path)
{
    NATIVE(native, path, INVALID_FILE_ATTRIBUTES);
    return realGetFileAttributesA(native);
}

static DWORD WINAPI hookGetFileAttributesW(LPCWSTR path)
{
    NATIVEW(native, path, INVALID_FILE_ATTRIBUTES);
    return realGetFileAttributesW(native);
}

static BOOL WINAPI hookGetFileAttributesExA(LPCSTR path,
                                            GET_FILEEX_INFO_LEVELS level,
                                            LPVOID info)
{
    NATIVE(native, path, FALSE);
    return realGetFileAttributesExA(native, level, info);
}

static BOOL WINAPI hookGetFileAttributesExW(LPCWSTR path,
                                            GET_FILEEX_INFO_LEVELS level,
                                            LPVOID info)
{
    NATIVEW(native, path, FALSE);
    return realGetFileAttributesExW(native, level, info);
}

#define HOOK_BOOL_A(hook, real)          \
    static BOOL WINAPI hook(LPCSTR path) \
    {                                    \
        NATIVE(native, path, FALSE);     \
        return real(native);             \
    }
#define HOOK_BOOL_W(hook, real)           \
    static BOOL WINAPI hook(LPCWSTR path) \
    {                                     \
        NATIVEW(native, path, FALSE);     \
        return real(native);              \
    }
HOOK_BOOL_A(hookDeleteFileA, realDeleteFileA)
HOOK_BOOL_W(hookDeleteFileW, realDeleteFileW)
HOOK_BOOL_A(hookRemoveDirectoryA, realRemoveDirectoryA)
HOOK_BOOL_W(hookRemoveDirectoryW, realRemoveDirectoryW)
HOOK_BOOL_A(hookSetCurrentDirectoryA, realSetCurrentDirectoryA)
HOOK_BOOL_W(hookSetCurrentDirectoryW, realSetCurrentDirectoryW)

static BOOL WINAPI hookCreateDirectoryA(LPCSTR path,
                                        LPSECURITY_ATTRIBUTES security)
{
    NATIVE(native, path, FALSE);
    return realCreateDirectoryA(native, security);
}

static BOOL WINAPI hookCreateDirectoryW(LPCWSTR path,
                                        LPSECURITY_ATTRIBUTES security)
{
    NATIVEW(native, path, FALSE);
    return realCreateDirectoryW(native, security);
}

static BOOL WINAPI hookMoveFileExA(LPCSTR from, LPCSTR to, DWORD flags)
{
    NATIVE(nativeFrom, from, FALSE);
    NATIVE(nativeTo, to, FALSE);
    return realMoveFileExA(nativeFrom, nativeTo, flags);
}

static BOOL WINAPI hookMoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags)
{
    NATIVEW(nativeFrom, from, FALSE);
    NATIVEW(nativeTo, to, FALSE);
    return realMoveFileExW(nativeFrom, nativeTo, flags);
}

static DWORD WINAPI hookGetFullPathNameA(LPCSTR path,
                                         DWORD size,
                                         LPSTR out,
                                         LPSTR *file)
{
    NATIVE(native, path, 0);
    return realGetFullPathNameA(native, size, out, file);
}

static DWORD WINAPI hookGetFullPathNameW(LPCWSTR path,
                                         DWORD size,
                                         LPWSTR out,
                                         LPWSTR *file)
{
    NATIVEW(native, path, 0);
    return realGetFullPathNameW(native, size, out, file);
}

static HANDLE WINAPI hookFindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
    NATIVE(native, pattern, INVALID_HANDLE_VALUE);
    return realFindFirstFileA(native, data);
}

static HANDLE WINAPI hookFindFirstFileW(LPCWSTR pattern,
                                        LPWIN32_FIND_DATAW data)
{
    NATIVEW(native, pattern, INVALID_HANDLE_VALUE);
    return realFindFirstFileW(native, data);
}

typedef struct Hook {
    const char *module; /* the DLL exporting the original */
    const char *name;
    void **real;
    void *hook;
} Hook;

#define CRT_HOOK(name, real, hook)                \
    {                                             \
        "msvcrt.dll", name, (void **) &real, hook \
    }
#define K32_HOOK(name, hook)                               \
    {                                                      \
        "kernel32.dll", #name, (void **) &real##name, hook \
    }
static Hook hooks[] = {
    CRT_HOOK("fopen", realFopen, hookFopen),
    CRT_HOOK("freopen", realFreopen, hookFreopen),
    CRT_HOOK("_open", realOpen, hookOpen),
    CRT_HOOK("_creat", realCreat, hookCreat),
    CRT_HOOK("_access", realAccess, hookAccess),
    CRT_HOOK("_chdir", realChdir, hookChdir),
    CRT_HOOK("_unlink", realUnlink, hookUnlink),
    CRT_HOOK("remove", realRemove, hookRemove),
    CRT_HOOK("rename", realRename, hookRename),
    CRT_HOOK("_rmdir", realRmdir, hookRmdir),
    CRT_HOOK("_mkdir", realMkdir, hookMkdir),
    CRT_HOOK("_stat", realStat, hookStat),
    CRT_HOOK("_stati64", realStati64, hookStati64),
    CRT_HOOK("_stat64", realStat64, hookStat64),
    CRT_HOOK("_stat32", realStat32, hookStat32),
    CRT_HOOK("_stat32i64", realStat32i64, hookStat32i64),
    CRT_HOOK("_stat64i32", realStat64i32, hookStat64i32),
    CRT_HOOK("_fullpath", realFullpath, hookFullpath),
    CRT_HOOK("_wfopen", realWfopen, hookWfopen),
    CRT_HOOK("_wopen", realWopen, hookWopen),
    CRT_HOOK("_wcreat", realWcreat, hookWcreat),
    CRT_HOOK("_waccess", realWaccess, hookWaccess),
    CRT_HOOK("_wchdir", realWchdir, hookWchdir),
    CRT_HOOK("_wunlink", realWunlink, hookWunlink),
    CRT_HOOK("_wremove", realWremove, hookWremove),
    CRT_HOOK("_wrename", realWrename, hookWrename),
    CRT_HOOK("_wrmdir", realWrmdir, hookWrmdir),
    CRT_HOOK("_wmkdir", realWmkdir, hookWmkdir),
    CRT_HOOK("_wstat", realWstat, hookWstat),
    CRT_HOOK("_wstati64", realWstati64, hookWstati64),
    CRT_HOOK("_wstat64", realWstat64, hookWstat64),
    CRT_HOOK("_wstat32", realWstat32, hookWstat32),
    CRT_HOOK("_wstat32i64", realWstat32i64, hookWstat32i64),
    CRT_HOOK("_wstat64i32", realWstat64i32, hookWstat64i32),
    CRT_HOOK("_wfullpath", realWfullpath, hookWfullpath),
    K32_HOOK(CreateFileA, hookCreateFileA),
    K32_HOOK(CreateFileW, hookCreateFileW),
    K32_HOOK(GetFileAttributesA, hookGetFileAttributesA),
    K32_HOOK(GetFileAttributesW, hookGetFileAttributesW),
    K32_HOOK(GetFileAttributesExA, hookGetFileAttributesExA),
    K32_HOOK(GetFileAttributesExW, hookGetFileAttributesExW),
    K32_HOOK(DeleteFileA, hookDeleteFileA),
    K32_HOOK(DeleteFileW, hookDeleteFileW),
    K32_HOOK(RemoveDirectoryA, hookRemoveDirectoryA),
    K32_HOOK(RemoveDirectoryW, hookRemoveDirectoryW),
    K32_HOOK(SetCurrentDirectoryA, hookSetCurrentDirectoryA),
    K32_HOOK(SetCurrentDirectoryW, hookSetCurrentDirectoryW),
    K32_HOOK(CreateDirectoryA, hookCreateDirectoryA),
    K32_HOOK(CreateDirectoryW, hookCreateDirectoryW),
    K32_HOOK(MoveFileExA, hookMoveFileExA),
    K32_HOOK(MoveFileExW, hookMoveFileExW),
    K32_HOOK(GetFullPathNameA, hookGetFullPathNameA),
    K32_HOOK(GetFullPathNameW, hookGetFullPathNameW),
    K32_HOOK(FindFirstFileA, hookFindFirstFileA),
    K32_HOOK(FindFirstFileW, hookFindFirstFileW),
};

#define HOOK_COUNT (sizeof(hooks) / sizeof(hooks[0]))

/* The originals' addresses, as the loader wrote them into import tables
 * (GetProcAddress follows kernel32's forwarders to kernelbase as the loader
 * does). An absent export (an older msvcrt) leaves its hook unused. */
static void resolveOriginals(void)
{
    for (size_t i = 0; i < HOOK_COUNT; i++) {
        HMODULE module = GetModuleHandleA(hooks[i].module);
        if (module && !*hooks[i].real)
            *hooks[i].real = (void *) GetProcAddress(module, hooks[i].name);
    }
}

static int importsUcrt(BYTE *base, IMAGE_IMPORT_DESCRIPTOR *imports)
{
    for (; imports->Name; imports++) {
        const char *dll = (const char *) (base + imports->Name);
        if (!_strnicmp(dll, "ucrtbase", 8) ||
            !_strnicmp(dll, "api-ms-win-crt-", 15))
            return 1;
    }
    return 0;
}

static void patchModule(HMODULE module)
{
    BYTE *base = (BYTE *) module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *) base;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_IMPORT_DESCRIPTOR *imports;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return;
    nt = (IMAGE_NT_HEADERS *) (base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return;
    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress || !dir->Size)
        return;
    imports = (IMAGE_IMPORT_DESCRIPTOR *) (base + dir->VirtualAddress);
    if (importsUcrt(base, imports))
        return;
    for (; imports->Name; imports++) {
        IMAGE_THUNK_DATA *thunk =
            (IMAGE_THUNK_DATA *) (base + imports->FirstThunk);
        for (; thunk->u1.Function; thunk++) {
            void *target = (void *) (ULONG_PTR) thunk->u1.Function;
            for (size_t i = 0; i < HOOK_COUNT; i++) {
                DWORD old;
                if (!*hooks[i].real || target != *hooks[i].real)
                    continue;
                if (VirtualProtect(&thunk->u1.Function, sizeof(void *),
                                   PAGE_READWRITE, &old)) {
                    thunk->u1.Function = (ULONG_PTR) hooks[i].hook;
                    VirtualProtect(&thunk->u1.Function, sizeof(void *), old,
                                   &old);
                }
                break;
            }
        }
    }
}

/* Under the Windows directory (case-insensitively): a system DLL. */
static int isSystemModule(HMODULE module, const wchar_t *windowsDir)
{
    wchar_t path[MAX_PATH];
    size_t len = wcslen(windowsDir);
    DWORD got = GetModuleFileNameW(module, path, MAX_PATH);
    return got && got < MAX_PATH && len &&
           _wcsnicmp(path, windowsDir, len) == 0;
}

void x11compatPatchImports(void)
{
    HMODULE modules[1024], self = NULL;
    DWORD needed = 0;
    wchar_t windowsDir[MAX_PATH];
    UINT dirLen;

    resolveOriginals();
    dirLen = GetSystemWindowsDirectoryW(windowsDir, MAX_PATH);
    if (!dirLen || dirLen >= MAX_PATH)
        windowsDir[0] = L'\0';
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR) (void *) x11compatPatchImports, &self);
    if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules),
                            &needed))
        return;
    if (needed > sizeof(modules))
        needed = sizeof(modules);
    for (DWORD i = 0; i < needed / sizeof(HMODULE); i++) {
        if (modules[i] == self || isSystemModule(modules[i], windowsDir))
            continue;
        patchModule(modules[i]);
    }
}

/* At load: every module the program was linked against is mapped, and its
 * imports bound, before any DLL initializer runs. */
__attribute__((constructor)) static void patchImportsAtLoad(void)
{
    x11compatPatchImports();
}
