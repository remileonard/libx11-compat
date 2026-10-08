/*
 * Minimal <poll.h> for the Windows (MinGW-w64) build: POSIX poll() over
 * WSAPoll, for the toolkits (libXt's event loop) that poll ConnectionNumber().
 *
 * WSAPoll only takes sockets, which is what ConnectionNumber() is on Windows
 * (src/event-pipe.h). Its WSAPOLLFD holds an 8-byte SOCKET where POSIX has an
 * int fd, so the two structs cannot be cast to each other: this header keeps
 * the POSIX layout under its own tag, and compat/win32/poll.c converts. Kept
 * free of <winsock2.h>, which collides with the X headers.
 */
#ifndef LIBX11_COMPAT_WIN32_POLL_H
#define LIBX11_COMPAT_WIN32_POLL_H

/* Same values as <winsock2.h>, so either header may come first. */
#ifndef POLLRDNORM
#define POLLRDNORM 0x0100
#define POLLRDBAND 0x0200
#define POLLIN (POLLRDNORM | POLLRDBAND)
#define POLLPRI 0x0400
#define POLLWRNORM 0x0010
#define POLLOUT (POLLWRNORM)
#define POLLWRBAND 0x0020
#define POLLERR 0x0001
#define POLLHUP 0x0002
#define POLLNVAL 0x0004
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Renamed so it never clashes with WSAPoll's struct pollfd. */
#define pollfd x11compat_pollfd
#define poll x11compatPoll

struct x11compat_pollfd {
    int fd;
    short events;
    short revents;
};

typedef unsigned long nfds_t;

int x11compatPoll(struct x11compat_pollfd *fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_POLL_H */
