/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_impl.h"

int32_t __fastcall __psx_state_lock(nt_sync_block * sync, uint32_t flags)
{
	int32_t status;

	/* simple case */
	if (!(status = __ntapi->tt_sync_block_lock(
			sync,
			sync->lock_tries,
			sync->lock_wait.quad,
			0)))
		return status;

	/* todo: daemon-based arbitration */
	return status;
}
