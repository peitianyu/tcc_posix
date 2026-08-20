/* midipix_tcc: size_t/ptrdiff_t/wchar_t from psxtypes + fallbacks */
#ifndef _MIDIPIX_TCC_STDDEF_H
#define _MIDIPIX_TCC_STDDEF_H
#include <psxtypes/psxtypes.h>
#ifndef NULL
#define NULL ((void *)0)
#endif
/* TCC's own stddef.h would normally provide wchar_t; ours replaces it */
#ifndef __WCHAR_T_DEFINED
#define __WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#endif
#ifndef _WINT_T
#define _WINT_T
typedef unsigned int wint_t;
#endif
#endif
