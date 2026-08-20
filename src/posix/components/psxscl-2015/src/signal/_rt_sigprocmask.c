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

__psx_api
int __psx_rt_sigprocmask_count = 0;
int __psx_rt_sigprocmask_last_how = -1;

intptr_t __sys_rt_sigprocmask(int how, const sigset_t * set, sigset_t * oldset)
{
	__psx_rt_sigprocmask_count++;
	__psx_rt_sigprocmask_last_how = how;
	return 0;
}
