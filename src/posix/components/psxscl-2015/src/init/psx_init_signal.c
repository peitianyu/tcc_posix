/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_signal.h"
#include "psx_impl.h"
#include "psx.h"

int __psx_init_signal(struct __psx_ctx * ctx)
{
	int32_t	status;
	int	i;
	nt_oa	oa = {sizeof(oa)};

	for (i=0; i<__PSX_ITIMER_CAP; i++)
		if ((status = __ntapi->zw_create_timer(
				&ctx->timer[i].htimer,
				NT_TIMER_ALL_ACCESS,
				&oa,NT_NOTIFICATION_TIMER)))
			return status;

	return status;
}
