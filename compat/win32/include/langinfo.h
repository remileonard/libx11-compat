/*
 * Minimal <langinfo.h> for the Windows (MinGW-w64) build: the two items
 * toolkits ask for (Motif's XmScale wants the radix character), answered from
 * the C runtime's locale.
 */
#ifndef LIBX11_COMPAT_WIN32_LANGINFO_H
#define LIBX11_COMPAT_WIN32_LANGINFO_H

#include <locale.h>

typedef int nl_item;

#define CODESET 0
#define RADIXCHAR 1

static inline char *nl_langinfo(nl_item item)
{
    if (item == RADIXCHAR) {
        struct lconv *conv = localeconv();
        return conv && conv->decimal_point ? conv->decimal_point : (char *) ".";
    }
    /* The compat stack speaks UTF-8 whatever the console code page. */
    return (char *) (item == CODESET ? "UTF-8" : "");
}

#endif /* LIBX11_COMPAT_WIN32_LANGINFO_H */
