/*
 * Constants MM's own libc headers (include/libc) define and musl doesn't, or
 * defines differently. Force-included first (Makefile.s30); the system
 * headers come in here so their include guards keep these definitions.
 */
#ifndef S30_LIBC_COMPAT_H
#define S30_LIBC_COMPAT_H

#include <float.h>
#include <limits.h>
#include <math.h>

#define M_PIf 3.14159265358979323846f
#define M_SQRT2f 1.41421356237309504880f
#define M_SQRT1_2f 0.70710678118654752440f /* 1/sqrt(2) */

/* MM's SHRT_MAX is a float: x / SHRT_MAX must stay a float division */
#undef SHRT_MAX
#define SHRT_MAX 32767.0f

#endif
