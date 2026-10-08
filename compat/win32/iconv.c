/* iconv over the Win32 codepage converters; see compat/win32/include/iconv.h.
 */
#include <iconv.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <windows.h>

/* Pseudo codepage for wchar_t text (UTF-16LE on Windows). */
#define CP_WIDE 1200

typedef struct {
    UINT to;
    UINT from;
} Converter;

/* Encoding name -> codepage, or 0 if unknown. Case, '-' and '_' are not
 * significant, and anything from "//" on is dropped. */
static UINT codepageForName(const char *name)
{
    char key[64];
    size_t n = 0;
    for (const char *p = name; *p && n + 1 < sizeof(key); p++) {
        if (p[0] == '/' && p[1] == '/')
            break;
        if (*p == '-' || *p == '_')
            continue;
        key[n++] = (char) toupper((unsigned char) *p);
    }
    key[n] = '\0';

    if (!strcmp(key, "UTF8"))
        return CP_UTF8;
    if (!strcmp(key, "WCHART") || !strcmp(key, "UTF16") ||
        !strcmp(key, "UTF16LE") || !strcmp(key, "UCS2") ||
        !strcmp(key, "UCS2LE"))
        return CP_WIDE;
    if (!strcmp(key, "ASCII") || !strcmp(key, "USASCII") ||
        !strcmp(key, "ANSIX3.41968") || !strcmp(key, "C") ||
        !strcmp(key, "POSIX"))
        return 20127;
    if (!strcmp(key, "LATIN1"))
        return 28591;
    if (!strncmp(key, "ISO8859", 7) && key[7]) {
        int part = atoi(key + 7);
        return part >= 1 && part <= 15 ? 28590 + part : 0;
    }
    /* A bare number is a codepage: the CRT names locales "Language.1252". */
    if (isdigit((unsigned char) key[0]))
        return (UINT) atoi(key);
    if (!strncmp(key, "CP", 2) && isdigit((unsigned char) key[2]))
        return (UINT) atoi(key + 2);
    if (!strncmp(key, "WINDOWS", 7) && isdigit((unsigned char) key[7]))
        return (UINT) atoi(key + 7);
    return 0;
}

iconv_t iconv_open(const char *tocode, const char *fromcode)
{
    UINT to = tocode ? codepageForName(tocode) : 0;
    UINT from = fromcode ? codepageForName(fromcode) : 0;
    if (!to || !from) {
        errno = EINVAL;
        return (iconv_t) -1;
    }
    Converter *cd = malloc(sizeof(*cd));
    if (!cd) {
        errno = ENOMEM;
        return (iconv_t) -1;
    }
    cd->to = to;
    cd->from = from;
    return cd;
}

int iconv_close(iconv_t cd)
{
    free(cd);
    return 0;
}

size_t iconv(iconv_t cd,
             char **inbuf,
             size_t *inbytesleft,
             char **outbuf,
             size_t *outbytesleft)
{
    const Converter *conv = cd;
    /* Stateless converters: a flush or reset has nothing to do. */
    if (!inbuf || !*inbuf || !*inbytesleft)
        return 0;
    if (*inbytesleft > INT_MAX) {
        errno = E2BIG;
        return (size_t) -1;
    }

    /* Decode to UTF-16 first, unless the input already is. */
    WCHAR *wide = NULL;
    const WCHAR *wideIn;
    int wideLen;
    if (conv->from == CP_WIDE) {
        if (*inbytesleft % sizeof(WCHAR)) {
            errno = EINVAL;
            return (size_t) -1;
        }
        wideIn = (const WCHAR *) *inbuf;
        wideLen = (int) (*inbytesleft / sizeof(WCHAR));
    } else {
        int inLen = (int) *inbytesleft;
        wideLen = MultiByteToWideChar(conv->from, 0, *inbuf, inLen, NULL, 0);
        if (wideLen <= 0) {
            errno = EILSEQ;
            return (size_t) -1;
        }
        wide = malloc((size_t) wideLen * sizeof(WCHAR));
        if (!wide) {
            errno = ENOMEM;
            return (size_t) -1;
        }
        MultiByteToWideChar(conv->from, 0, *inbuf, inLen, wide, wideLen);
        wideIn = wide;
    }

    /* Then encode, all or nothing. */
    size_t outLen;
    if (conv->to == CP_WIDE) {
        outLen = (size_t) wideLen * sizeof(WCHAR);
        if (outLen <= *outbytesleft)
            memcpy(*outbuf, wideIn, outLen);
    } else {
        int need = WideCharToMultiByte(conv->to, 0, wideIn, wideLen, NULL, 0,
                                       NULL, NULL);
        if (need <= 0) {
            free(wide);
            errno = EILSEQ;
            return (size_t) -1;
        }
        outLen = (size_t) need;
        if (outLen <= *outbytesleft)
            WideCharToMultiByte(conv->to, 0, wideIn, wideLen, *outbuf, need,
                                NULL, NULL);
    }
    free(wide);
    if (outLen > *outbytesleft) {
        errno = E2BIG;
        return (size_t) -1;
    }
    *inbuf += *inbytesleft;
    *inbytesleft = 0;
    *outbuf += outLen;
    *outbytesleft -= outLen;
    return 0;
}
