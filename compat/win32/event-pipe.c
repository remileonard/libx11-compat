/*
 * Windows event wake pipe behind ConnectionNumber() (see src/event-pipe.h):
 * two connected, non-blocking loopback TCP sockets, because Windows can only
 * select()/WSAPoll() on sockets. Kept out of src/events.c so <winsock2.h>
 * never meets the X headers.
 */
#include <winsock2.h>
#include <ws2tcpip.h>

#include <sys/types.h>

#include "event-pipe.h"

/* SOCKET handles are small kernel handle values; they round-trip through the
 * int ConnectionNumber() returns, as in Xlib's own Windows port. */
int eventPipeCreate(int fds[2])
{
    static int wsaReady;
    if (!wsaReady) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return -1;
        wsaReady = 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        return -1;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int addrLen = sizeof(addr);
    SOCKET writer = INVALID_SOCKET, reader = INVALID_SOCKET;
    if (bind(listener, (struct sockaddr *) &addr, sizeof(addr)) != 0 ||
        getsockname(listener, (struct sockaddr *) &addr, &addrLen) != 0 ||
        listen(listener, 1) != 0)
        goto fail;
    writer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (writer == INVALID_SOCKET ||
        connect(writer, (struct sockaddr *) &addr, sizeof(addr)) != 0)
        goto fail;
    reader = accept(listener, NULL, NULL);
    if (reader == INVALID_SOCKET)
        goto fail;
    closesocket(listener);

    /* Wake bytes are single characters: send them immediately. */
    BOOL noDelay = TRUE;
    setsockopt(writer, IPPROTO_TCP, TCP_NODELAY, (const char *) &noDelay,
               sizeof(noDelay));
    u_long nonBlocking = 1;
    ioctlsocket(reader, FIONBIO, &nonBlocking);
    ioctlsocket(writer, FIONBIO, &nonBlocking);
    fds[0] = (int) reader;
    fds[1] = (int) writer;
    return 0;

fail:
    if (writer != INVALID_SOCKET)
        closesocket(writer);
    closesocket(listener);
    return -1;
}

ssize_t eventPipeRead(int fd, void *buffer, size_t size)
{
    int n = recv((SOCKET) fd, (char *) buffer, (int) size, 0);
    return n == SOCKET_ERROR ? -1 : n;
}

ssize_t eventPipeWrite(int fd, const void *buffer, size_t size)
{
    int n = send((SOCKET) fd, (const char *) buffer, (int) size, 0);
    return n == SOCKET_ERROR ? -1 : n;
}

void eventPipeClose(int fd)
{
    closesocket((SOCKET) fd);
}
