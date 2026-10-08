/* Glob-sized POSIX basic regex matcher; see compat/win32/include/regex.h. */
#include <regex.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int icase;
} Matcher;

static int sameChar(const Matcher *m, int a, int b)
{
    return m->icase ? tolower(a) == tolower(b) : a == b;
}

/* Length of the atom at `re` (a literal, '.', "\c" or "[...]"), or 0 when the
 * pattern is malformed (an unterminated bracket or trailing backslash). */
static size_t atomLength(const char *re)
{
    if (re[0] == '\\')
        return re[1] ? 2 : 0;
    if (re[0] != '[')
        return 1;
    size_t i = 1;
    if (re[i] == '^' || re[i] == '!')
        i++;
    if (re[i] == ']') /* a leading ']' is a member */
        i++;
    while (re[i] && re[i] != ']')
        i++;
    return re[i] == ']' ? i + 1 : 0;
}

/* Does the atom at `re` (of length `len`) match character `c`? */
static int atomMatches(const Matcher *m, const char *re, size_t len, int c)
{
    if (c == '\0')
        return 0;
    if (re[0] == '\\')
        return sameChar(m, re[1], c);
    if (re[0] == '.')
        return 1;
    if (re[0] != '[')
        return sameChar(m, re[0], c);

    size_t i = 1, end = len - 1;
    int negate = 0, found = 0;
    if (re[i] == '^' || re[i] == '!') {
        negate = 1;
        i++;
    }
    for (int first = 1; i < end; first = 0) {
        int lo = (unsigned char) re[i];
        if (lo == ']' && !first)
            break;
        if (i + 2 < end && re[i + 1] == '-') {
            int hi = (unsigned char) re[i + 2];
            if ((c >= lo && c <= hi) ||
                (m->icase && tolower(c) >= tolower(lo) &&
                 tolower(c) <= tolower(hi)))
                found = 1;
            i += 3;
        } else {
            if (sameChar(m, lo, c))
                found = 1;
            i++;
        }
    }
    return found != negate;
}

static int matchHere(const Matcher *m, const char *re, const char *text);

/* atom* at `re` followed by `rest`: try the longest run first. */
static int matchStar(const Matcher *m,
                     const char *atom,
                     size_t len,
                     const char *rest,
                     const char *text)
{
    const char *t = text;
    while (atomMatches(m, atom, len, (unsigned char) *t))
        t++;
    do {
        if (matchHere(m, rest, t))
            return 1;
    } while (t-- > text);
    return 0;
}

static int matchHere(const Matcher *m, const char *re, const char *text)
{
    for (;;) {
        if (re[0] == '\0')
            return 1;
        if (re[0] == '$' && re[1] == '\0')
            return *text == '\0';
        size_t len = atomLength(re);
        if (len == 0)
            return 0;
        if (re[len] == '*')
            return matchStar(m, re, len, re + len + 1, text);
        if (!atomMatches(m, re, len, (unsigned char) *text))
            return 0;
        re += len;
        text++;
    }
}

int regcomp(regex_t *preg, const char *pattern, int cflags)
{
    /* Validate once so regexec never meets a malformed atom. */
    for (const char *p = pattern[0] == '^' ? pattern + 1 : pattern; *p;) {
        if (p[0] == '$' && p[1] == '\0')
            break;
        size_t len = atomLength(p);
        if (len == 0)
            return REG_BADPAT;
        p += len;
        if (*p == '*')
            p++;
    }
    preg->re_pattern = strdup(pattern);
    if (!preg->re_pattern)
        return REG_ESPACE;
    preg->re_cflags = cflags;
    preg->re_nsub = 0;
    return 0;
}

int regexec(const regex_t *preg,
            const char *string,
            size_t nmatch,
            regmatch_t pmatch[],
            int eflags)
{
    (void) nmatch;
    (void) pmatch;
    (void) eflags;
    Matcher m = {(preg->re_cflags & REG_ICASE) != 0};
    const char *re = preg->re_pattern;
    if (re[0] == '^')
        return matchHere(&m, re + 1, string) ? 0 : REG_NOMATCH;
    const char *text = string;
    do {
        if (matchHere(&m, re, text))
            return 0;
    } while (*text++);
    return REG_NOMATCH;
}

size_t regerror(int errcode,
                const regex_t *preg,
                char *errbuf,
                size_t errbuf_size)
{
    (void) preg;
    const char *msg = errcode == REG_NOMATCH  ? "No match"
                      : errcode == REG_BADPAT ? "Invalid regular expression"
                      : errcode == REG_ESPACE ? "Out of memory"
                                              : "Regular expression error";
    if (errbuf && errbuf_size)
        snprintf(errbuf, errbuf_size, "%s", msg);
    return strlen(msg) + 1;
}

void regfree(regex_t *preg)
{
    free(preg->re_pattern);
    preg->re_pattern = NULL;
}
