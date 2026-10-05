/*
 * <sys/select.h> for X clients built for Windows against libx11-compat.
 *
 * ConnectionNumber() is a socket there (see src/event-pipe.h), so the classic
 * select() loop on the display connection works through Winsock. Winsock's
 * headers collide with the X headers (BOOL, Status, INT32, ...); xorgproto's
 * <X11/Xwinsock.h> is the standard wrapper that includes them safely, as in
 * Xlib's own Windows port.
 */
#ifndef LIBX11_COMPAT_WIN32_SYS_SELECT_H
#define LIBX11_COMPAT_WIN32_SYS_SELECT_H

#include <X11/Xwinsock.h>

#endif /* LIBX11_COMPAT_WIN32_SYS_SELECT_H */
