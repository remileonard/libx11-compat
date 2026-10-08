/*
 * <nl_types.h> for the Windows (MinGW-w64) build: X/Open message catalogs.
 * Windows has none, so catopen() always fails and catgets() returns the
 * caller's default string, exactly what POSIX code sees when a catalog is
 * missing (Open Inventor's textomatic demo).
 */
#ifndef LIBX11_COMPAT_WIN32_NL_TYPES_H
#define LIBX11_COMPAT_WIN32_NL_TYPES_H

#include <errno.h>

typedef void *nl_catd;
typedef int nl_item;

#define NL_SETD 1
#define NL_CAT_LOCALE 1

static inline nl_catd catopen(const char *name, int flag)
{
    (void) name;
    (void) flag;
    errno = ENOENT;
    return (nl_catd) -1;
}

static inline char *catgets(nl_catd catd, int set_id, int msg_id, const char *s)
{
    (void) catd;
    (void) set_id;
    (void) msg_id;
    return (char *) s;
}

static inline int catclose(nl_catd catd)
{
    (void) catd;
    return 0;
}

#endif /* LIBX11_COMPAT_WIN32_NL_TYPES_H */
