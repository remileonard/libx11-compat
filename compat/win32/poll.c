/* POSIX poll() over WSAPoll; see compat/win32/include/poll.h. */
#include <winsock2.h>

#include <errno.h>
#include <stdlib.h>

/* Declared by hand: <poll.h> would rename WSAPoll's struct pollfd. */
struct x11compat_pollfd {
    int fd;
    short events;
    short revents;
};

int x11compatPoll(struct x11compat_pollfd *fds, unsigned long nfds, int timeout)
{
    WSAPOLLFD stack[16];
    WSAPOLLFD *wfds = stack;
    unsigned long count = 0;

    if (nfds > sizeof(stack) / sizeof(stack[0])) {
        wfds = malloc(nfds * sizeof(*wfds));
        if (!wfds) {
            errno = ENOMEM;
            return -1;
        }
    }
    /* POSIX skips negative fds; WSAPoll would reject them. */
    for (unsigned long i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0)
            continue;
        wfds[count].fd = (SOCKET) (unsigned) fds[i].fd;
        wfds[count].events = fds[i].events & (POLLIN | POLLOUT | POLLPRI);
        wfds[count].revents = 0;
        count++;
    }

    int ready;
    if (count == 0) {
        /* WSAPoll rejects an empty set; POSIX poll() then just sleeps. */
        if (timeout > 0)
            Sleep((DWORD) timeout);
        ready = 0;
    } else {
        ready = WSAPoll(wfds, count, timeout);
        if (ready == SOCKET_ERROR) {
            errno = WSAGetLastError() == WSAEINTR ? EINTR : EINVAL;
            ready = -1;
        } else {
            unsigned long j = 0;
            for (unsigned long i = 0; i < nfds; i++) {
                if (fds[i].fd < 0)
                    continue;
                fds[i].revents = wfds[j++].revents;
            }
        }
    }
    if (wfds != stack)
        free(wfds);
    return ready;
}
