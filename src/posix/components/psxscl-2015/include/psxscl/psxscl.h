#ifndef _PSXSCL_H_
#define _PSXSCL_H_

#if   defined (PSXSCL_BUILD)
#define	__psx_api __attr_export__
#elif defined (PSXSCL_SHARED)
#define	__psx_api __attr_import__
#else
#define	__psx_api
#endif

#include "psxglue.h"

__psx_api
__psx_init_routine __psx_init;

#endif
