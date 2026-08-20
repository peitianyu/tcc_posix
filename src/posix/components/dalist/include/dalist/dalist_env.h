#ifndef DALIST_ENV_H
#define DALIST_ENV_H

#if defined (_MIDIPIX_FREESTANDING)

#include <psxtypes/psxtypes.h>

#elif defined (_DALIST_FREESTANDING)

#include <stdint.h>
typedef  intptr_t ssize_t;

#else

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>

#ifndef __cdecl
#define __cdecl
#endif

#ifndef __stdcall
#define __stdcall
#endif

#endif

#endif
