/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx.h"

/******************************/
/* todo: more thinking...     */
/* (already implemented)      */
/*  ntapi/src/vmount          */
/*  ntctty/src/vmount         */
/******************************/

__psx_api
intptr_t __sys_mount(
	const char *	source,
	const char *	target,
	const char *	fstype,
	uintptr_t	mntflags,
	const void *	data)
{
	return -ENOSYS;
}
