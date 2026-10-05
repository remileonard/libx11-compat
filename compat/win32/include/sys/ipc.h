/*
 * Minimal <sys/ipc.h> for the Windows build of libx11-compat. Windows has no
 * System V IPC; this only provides the names the MIT-SHM headers and shims
 * use. See <sys/shm.h> for the (always failing) segment calls.
 */
#ifndef LIBX11_COMPAT_WIN32_SYS_IPC_H
#define LIBX11_COMPAT_WIN32_SYS_IPC_H

typedef int key_t;

#define IPC_PRIVATE ((key_t) 0)
#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_RMID 0
#define IPC_SET 1
#define IPC_STAT 2

#endif /* LIBX11_COMPAT_WIN32_SYS_IPC_H */
