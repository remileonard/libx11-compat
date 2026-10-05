/*
 * The event wake pipe behind ConnectionNumber().
 *
 * Clients (and Xt) select()/poll() on ConnectionNumber() and then call
 * XNextEvent; libx11-compat writes one byte per queued event (plus timer wake
 * bytes) into a non-blocking pipe whose read end is that descriptor.
 *
 * POSIX uses pipe(2). Windows can only select()/WSAPoll() on sockets, so there
 * the pair is two connected loopback TCP sockets; the read end is handed out
 * as the connection number like an X client's socket would be.
 */
#ifndef LIBX11_COMPAT_EVENT_PIPE_H
#define LIBX11_COMPAT_EVENT_PIPE_H

#include <stddef.h>
#include <sys/types.h>

#ifdef _WIN32
/* Implemented in compat/win32/event-pipe.c, which can include <winsock2.h>
 * without colliding with the X headers (BOOL, Status, None, ...). SOCKET
 * handles are small kernel handle values; they round-trip through the int
 * ConnectionNumber() returns, as in Xlib's own Windows port.
 */
int eventPipeCreate(int fds[2]);
ssize_t eventPipeRead(int fd, void *buffer, size_t size);
ssize_t eventPipeWrite(int fd, const void *buffer, size_t size);
void eventPipeClose(int fd);

#else /* POSIX */
#include <fcntl.h>
#include <unistd.h>

static inline int eventPipeCreate(int fds[2])
{
    if (pipe(fds) == -1)
        return -1;
    /* Real X11 clients select() / poll() on the connection FD then call
     * XNextEvent in a non-blocking mode. The pipe is just a wake-up signal;
     * reads happen inside XNextEvent itself, so make the FD non-blocking so
     * a desynced qlen does not stall the whole event loop on a missing
     * byte. (F_SETFL / O_NONBLOCK, not F_SETFD / FD_CLOEXEC.)
     */
    for (int i = 0; i < 2; i++) {
        int flags = fcntl(fds[i], F_GETFL);
        fcntl(fds[i], F_SETFL, flags | O_NONBLOCK);
    }
    return 0;
}

static inline ssize_t eventPipeRead(int fd, void *buffer, size_t size)
{
    return read(fd, buffer, size);
}

static inline ssize_t eventPipeWrite(int fd, const void *buffer, size_t size)
{
    return write(fd, buffer, size);
}

static inline void eventPipeClose(int fd)
{
    close(fd);
}
#endif

#endif /* LIBX11_COMPAT_EVENT_PIPE_H */
