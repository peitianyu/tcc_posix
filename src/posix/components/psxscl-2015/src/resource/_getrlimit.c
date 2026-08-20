/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include "psx_systypes.h"
#include "psx_resource.h"
#include "psx_errno.h"
#include "psx.h"

__psx_api
intptr_t __sys_getrlimit(int resource, struct __rlimit * rlp)
{
	/* todo */
	return -ENOSYS;
}
