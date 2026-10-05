/*
 * Minimal <sys/shm.h> for the Windows build of libx11-compat. There are no
 * System V shared memory segments on Windows: every call fails with ENOSYS,
 * and XShmQueryExtension reports the extension absent there, so clients fall
 * back to XPutImage as they do against a server without MIT-SHM.
 */
#ifndef LIBX11_COMPAT_WIN32_SYS_SHM_H
#define LIBX11_COMPAT_WIN32_SYS_SHM_H

#include <errno.h>
#include <stddef.h>
#include <sys/ipc.h>

#define SHM_RDONLY 010000

struct shmid_ds {
    size_t shm_segsz;
    int shm_nattch;
};

static inline int shmget(key_t key, size_t size, int flags)
{
    (void) key;
    (void) size;
    (void) flags;
    errno = ENOSYS;
    return -1;
}

static inline void *shmat(int shmid, const void *addr, int flags)
{
    (void) shmid;
    (void) addr;
    (void) flags;
    errno = ENOSYS;
    return (void *) -1;
}

static inline int shmdt(const void *addr)
{
    (void) addr;
    errno = ENOSYS;
    return -1;
}

static inline int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    (void) shmid;
    (void) cmd;
    (void) buf;
    errno = ENOSYS;
    return -1;
}

#endif /* LIBX11_COMPAT_WIN32_SYS_SHM_H */
