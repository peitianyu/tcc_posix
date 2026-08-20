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
intptr_t __sys_prlimit(pid_t pid, int resource, const struct __rlimit * new_limit, struct __rlimit * old_limit)
{
	/* todo */
	if (old_limit && (resource == RLIMIT_STACK)) {
		old_limit->rlim_cur = 64*1024*1024;
		old_limit->rlim_max = 64*1024*1024;
	} else if (old_limit && (resource == RLIMIT_NOFILE)) {
		old_limit->rlim_cur = 1024;
		old_limit->rlim_max = 1024;
	} else if (old_limit) {
		old_limit->rlim_cur = (rlim_t)-1;
		old_limit->rlim_cur = (rlim_t)-1;
	}

	return 0;
}
