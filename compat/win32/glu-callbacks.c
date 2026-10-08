/* cdecl GLU callbacks on 32-bit Windows; see compat/win32/include/GL/glu.h.
 */
#if defined(_WIN32) && defined(__i386__)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef void(__stdcall *StdcallFn)(void);

/* An import stub (jmp dword ptr [__imp_name]) stands for the function its
 * import address table slot points at. */
static const void *followImportStub(const void *fn)
{
    const unsigned char *code = fn;
    if (code[0] == 0xFF && code[1] == 0x25) {
        const void *const *slot;
        memcpy(&slot, code + 2, sizeof(slot));
        return *slot;
    }
    return fn;
}

/* Is fn code from opengl32.dll or glu32.dll, i.e. already __stdcall? */
static int isWin32GlEntryPoint(const void *fn)
{
    HMODULE owner = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR) followImportStub(fn), &owner))
        return 0;
    return owner == GetModuleHandleA("opengl32.dll") ||
           owner == GetModuleHandleA("glu32.dll");
}

/* Thunks are never freed: one per (function, arity) a program registers. */
typedef struct {
    void (*fn)(void);
    int argWords;
    unsigned char *code;
} Thunk;

static SRWLOCK lock = SRWLOCK_INIT;
static Thunk *thunks;
static size_t thunkCount, thunkCapacity;
static unsigned char *page;
static size_t pageUsed, pageSize;

enum { THUNK_MAX_BYTES = 64 };

/* Emit: push the argWords arguments again (last first), call fn, drop
 * them, and return popping the caller's copies, as __stdcall requires. */
static unsigned char *emitThunk(void (*fn)(void), int argWords)
{
    if (!page || pageUsed + THUNK_MAX_BYTES > pageSize) {
        pageSize = 4096;
        page = VirtualAlloc(NULL, pageSize, MEM_COMMIT | MEM_RESERVE,
                            PAGE_EXECUTE_READWRITE);
        pageUsed = 0;
        if (!page)
            return NULL;
    }
    unsigned char *start = page + pageUsed, *p = start;
    for (int i = 0; i < argWords; i++) {
        *p++ = 0xFF; /* push dword ptr [esp + 4 * argWords] */
        *p++ = 0x74;
        *p++ = 0x24;
        *p++ = (unsigned char) (4 * argWords);
    }
    uint32_t target = (uint32_t) (uintptr_t) fn;
    *p++ = 0xB8; /* mov eax, fn */
    memcpy(p, &target, 4);
    p += 4;
    *p++ = 0xFF; /* call eax */
    *p++ = 0xD0;
    if (argWords > 0) {
        *p++ = 0x83; /* add esp, 4 * argWords */
        *p++ = 0xC4;
        *p++ = (unsigned char) (4 * argWords);
        *p++ = 0xC2; /* ret 4 * argWords */
        uint16_t pop = (uint16_t) (4 * argWords);
        memcpy(p, &pop, 2);
        p += 2;
    } else {
        *p++ = 0xC3; /* ret */
    }
    pageUsed += (size_t) (p - start);
    FlushInstructionCache(GetCurrentProcess(), start, (size_t) (p - start));
    return start;
}

StdcallFn x11compatGluCallback(void (*fn)(void), int argWords);

StdcallFn x11compatGluCallback(void (*fn)(void), int argWords)
{
    StdcallFn same;
    memcpy(&same, &fn, sizeof(same));
    if (!fn || argWords < 0 || argWords > 31 ||
        isWin32GlEntryPoint((const void *) (uintptr_t) fn))
        return same;

    AcquireSRWLockExclusive(&lock);
    unsigned char *code = NULL;
    for (size_t i = 0; i < thunkCount; i++)
        if (thunks[i].fn == fn && thunks[i].argWords == argWords)
            code = thunks[i].code;
    if (!code && thunkCount == thunkCapacity) {
        size_t capacity = thunkCapacity ? thunkCapacity * 2 : 16;
        Thunk *grown = realloc(thunks, capacity * sizeof(*grown));
        if (grown) {
            thunks = grown;
            thunkCapacity = capacity;
        }
    }
    if (!code && thunkCount < thunkCapacity) {
        code = emitThunk(fn, argWords);
        if (code) {
            thunks[thunkCount].fn = fn;
            thunks[thunkCount].argWords = argWords;
            thunks[thunkCount].code = code;
            thunkCount++;
        }
    }
    ReleaseSRWLockExclusive(&lock);

    if (!code)
        return same;
    StdcallFn thunk;
    memcpy(&thunk, &code, sizeof(thunk));
    return thunk;
}

#else
/* 64-bit Windows has one calling convention: nothing to adapt. */
typedef int glu_callbacks_unused;
#endif
