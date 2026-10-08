/*
 * Minimal <iconv.h> for the Windows (MinGW-w64) build, over the Win32
 * codepage converters (compat/win32/iconv.c). MinGW has no iconv, and Motif
 * needs one for its XmString conversions between the locale charset, UTF-8
 * and wchar_t. Supported names: UTF-8, WCHAR_T / UTF-16(LE) / UCS-2, ASCII,
 * ISO-8859-n, CPnnn / WINDOWS-nnn / bare nnn; a "//IGNORE"-style suffix is
 * accepted.
 *
 * A call converts all of its input or none of it: on E2BIG nothing is
 * consumed, so a caller that grows its buffer and retries (Motif does)
 * sees the whole conversion on the retry.
 */
#ifndef LIBX11_COMPAT_WIN32_ICONV_H
#define LIBX11_COMPAT_WIN32_ICONV_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *iconv_t;

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t iconv(iconv_t cd,
             char **inbuf,
             size_t *inbytesleft,
             char **outbuf,
             size_t *outbytesleft);
int iconv_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_ICONV_H */
