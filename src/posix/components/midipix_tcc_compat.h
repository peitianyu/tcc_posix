/*
 * midipix_tcc_compat.h
 * Minimal TCC compatibility shim for the midipix 2015-era source tree.
 *
 * The midipix toolchain normally provides:
 *   - psxtypes compiler headers (selected via __GNUC__ / _MSC_VER),
 *   - SAL-style annotation macros,
 *   - __declspec / __attribute__ handling.
 *
 * With this file + the build flags below, TinyCC 0.9.27 can compile
 * psxscl/ntapi/mmglue source files.
 *
 * Required build flags (see build_psxscl.sh):
 *   -D__NT64 -D__GNUC__=4 -D__amd64=1 -D__SIZEOF_POINTER__=8
 *   -U_WIN32 -UWIN32 -U__WIN32__ -U_WIN64 -UWIN64 -U__WIN64__
 *   -ffreestanding -fno-builtin
 */
#ifndef MIDIPIX_TCC_COMPAT_H
#define MIDIPIX_TCC_COMPAT_H

#if defined(__TINYC__)
/* tcc 0.9.27 has no __declspec; null it out (dllexport/dllimport are
 * already expressed via __attribute__((dllexport/dllimport)) in the
 * psxtypes gcc compiler header). */
#define __declspec(x)
#endif

#endif /* MIDIPIX_TCC_COMPAT_H */
