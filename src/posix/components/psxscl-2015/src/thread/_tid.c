/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/ntapi.h>
#include "psx_tlca.h"
#include "psx.h"

__psx_api
intptr_t __sys_gettid(void)
{
	return pe_get_current_thread_id();
}

__psx_api
intptr_t __sys_set_tid_address(int * tidptr)
{
	__tlca_self()->pthread_clear_child_tid = tidptr;
	return pe_get_current_thread_id();
}
