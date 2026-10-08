/*
 * <values.h> for the Windows (MinGW-w64) build: the System V limits header
 * glibc still ships, which old code (Open Inventor's calculator engine) uses
 * for MAXFLOAT and friends. Defined from <float.h> and <limits.h>.
 */
#ifndef LIBX11_COMPAT_WIN32_VALUES_H
#define LIBX11_COMPAT_WIN32_VALUES_H

#include <float.h>
#include <limits.h>

#define BITSPERBYTE CHAR_BIT
#define MAXSHORT SHRT_MAX
#define MAXINT INT_MAX
#define MAXLONG LONG_MAX
#ifndef MAXFLOAT
#define MAXFLOAT FLT_MAX
#endif
#define MAXDOUBLE DBL_MAX
#define MINFLOAT FLT_MIN
#define MINDOUBLE DBL_MIN

#endif /* LIBX11_COMPAT_WIN32_VALUES_H */
