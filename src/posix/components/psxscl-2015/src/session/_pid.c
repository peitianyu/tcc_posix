/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include "psx_systypes.h"
#include "psx_session.h"
#include "psx_tlca.h"
#include "psx_session.h"
#include "psx_helper.h"
#include "psx_errno.h"
#include "psx_debug.h"
#include "psx.h"

enum __pid_type {
	__PID,
	__PPID,
	__PGID
};

/* todo: hastily written and possibly wrong */

static intptr_t __pid_get_set(pid_t pid, int32_t pgid, enum __pid_type type, int fset)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __process_record*rec;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (pid && !(rec = __psx_get_process_record(&ctx->offsprings,0,0,0,pid,0)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (fset && (pid == 0)) {
		rtdata->alt_cid_self.pgid = (pgid ? pgid : rtdata->alt_cid_self.pid);
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	}

	if (fset) {
		rec->pgid = pgid ? pgid : rec->pid;
		/* todo! lpc message... */
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	}

	if (type == __PGID)
		return __psx_sig_epilog(
			tlca,
			pid ? rec->pgid : rtdata->alt_cid_self.pgid,
			NT_STATUS_SUCCESS);

	if (type == __PPID)
		return __psx_sig_epilog(tlca,rtdata->alt_cid_self.pgid,NT_STATUS_SUCCESS);

	if (type == __PID)
		return __psx_sig_epilog(tlca,rtdata->alt_cid_self.pid,NT_STATUS_SUCCESS);

	return __psx_sig_epilog(tlca,-1,NT_STATUS_INTERNAL_ERROR);
}

__psx_api
intptr_t __sys_getpid(void)
{
	return __pid_get_set(0,0,__PID,0);
}

__psx_api
intptr_t __sys_getppid(void)
{
	return __pid_get_set(0,0,__PPID,0);
}

__psx_api
intptr_t __sys_getpgid(pid_t pid)
{
	return __pid_get_set(pid,0,__PGID,0);
}

__psx_api
intptr_t __sys_getpgrp(pid_t pid)
{
	return __pid_get_set(pid,0,__PGID,0);
}

__psx_api
intptr_t __sys_setpgid(pid_t pid, pid_t pgid)
{
	return __pid_get_set(pid,pgid,__PGID,1);
}
