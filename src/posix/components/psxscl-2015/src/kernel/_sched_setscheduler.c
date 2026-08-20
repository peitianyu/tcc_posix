/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx.h"

/* easy, low-priority */

__psx_api
intptr_t __sys_sched_setscheduler(
	pid_t				pid,
	int				policy,
	const struct sched_param *	param)
{
	return 0;
}
