/*
 * Minimal POSIX <regex.h> for the Windows (MinGW-w64) build, which has none.
 * compat/win32/regex.c implements the basic-regex subset toolkits generate
 * from file globs (Motif's file selection box turns "*.c" into "^.*\.c$"):
 * literals, '.', '*', '\' escapes, bracket expressions with ranges and
 * negation, and the '^' / '$' anchors. Matching is whole-string search
 * with no sub-match reporting, as REG_NOSUB callers expect.
 */
#ifndef LIBX11_COMPAT_WIN32_REGEX_H
#define LIBX11_COMPAT_WIN32_REGEX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NOSUB 4
#define REG_NEWLINE 8

#define REG_NOMATCH 1
#define REG_BADPAT 2
#define REG_ESPACE 12

typedef long regoff_t;

typedef struct {
    size_t re_nsub;
    char *re_pattern; /* private: the compiled (copied) pattern */
    int re_cflags;    /* private */
} regex_t;

typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;

int regcomp(regex_t *preg, const char *pattern, int cflags);
int regexec(const regex_t *preg,
            const char *string,
            size_t nmatch,
            regmatch_t pmatch[],
            int eflags);
size_t regerror(int errcode,
                const regex_t *preg,
                char *errbuf,
                size_t errbuf_size);
void regfree(regex_t *preg);

#ifdef __cplusplus
}
#endif

#endif /* LIBX11_COMPAT_WIN32_REGEX_H */
