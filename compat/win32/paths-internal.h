/* Shared by compat/win32/paths.c and compat/win32/imports.c (both linked
 * into libX11-compat.dll only). */
#ifndef LIBX11_COMPAT_WIN32_PATHS_INTERNAL_H
#define LIBX11_COMPAT_WIN32_PATHS_INTERNAL_H

#include <wchar.h>

/* Size, in characters, of the buffers the conversions below write to. */
#define PATH_BUFFER_SIZE 4096

/* "/c/x" -> "C:/x" into out[PATH_BUFFER_SIZE]; other paths pass through.
 * NULL, with errno ENAMETOOLONG, when the result does not fit. */
const char *x11compatToNativePath(const char *path, char *out);
const wchar_t *x11compatToNativePathW(const wchar_t *path, wchar_t *out);

/* X11COMPAT_PATHS=native: getcwd() and the home directory stay native
 * ("C:/Users/me") instead of "/c/Users/me". */
int x11compatNativePaths(void);

#endif /* LIBX11_COMPAT_WIN32_PATHS_INTERNAL_H */
