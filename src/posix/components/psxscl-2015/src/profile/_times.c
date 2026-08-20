/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_profile.h"
#include "psx_time.h"
#include "psx_tlca.h"
#include "psx_impl.h"
#include "psx.h"

__psx_api
clock_t __sys_times(struct tms * tms)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	nt_kernel_user_times	kut;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((status = __ntapi->zw_query_information_process(
			NT_CURRENT_PROCESS_HANDLE,
			NT_PROCESS_TIMES,
			&kut,sizeof(kut),0)))
		return __psx_sig_epilog(tlca,-ENOSYS,status);

	tms->tms_utime = (clock_t)(kut.user_time.quad / 100000);
	tms->tms_stime = (clock_t)(kut.kernel_time.quad / 100000);

	/* todo: add time spent in child processes */
	tms->tms_cutime = 0;
	tms->tms_cstime = 0;

	return __psx_sig_epilog(tlca,tms->tms_utime,status);
}
