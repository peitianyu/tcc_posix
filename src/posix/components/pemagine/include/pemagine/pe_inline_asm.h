#ifndef _PE_INLINE_ASM_H_
#define _PE_INLINE_ASM_H_

#include "pe_api.h"

#ifdef  _MSC_VER

/* visual studio: begin */
#ifdef  _M_IX86
#ifndef __SIZEOF_POINTER__
#define __SIZEOF_POINTER__ 4
#endif
#endif

#ifdef _M_X64
#ifndef __SIZEOF_POINTER__
#define __SIZEOF_POINTER__ 8
#endif
#endif

#if (__SIZEOF_POINTER__ == 4)
#include "bits/nt32/pe_inline_asm__msvc.h"
#endif

#if (__SIZEOF_POINTER__ == 8)
#include "bits/nt64/pe_inline_asm__msvc.h"
#endif
/* visual studio: end */

#else

/* all other compilers: begin */
#if (__SIZEOF_POINTER__ == 4)
#include "bits/nt32/pe_inline_asm__common.h"
#endif

#if (__SIZEOF_POINTER__ == 8)
#include "bits/nt64/pe_inline_asm__common.h"
#endif
/* all other compilers: end */

#endif

/* trivial */
static __inline__ void * pe_va_from_rva(const void * base, intptr_t offset)
{
        return (void *)((intptr_t)base + offset);
}

#endif
