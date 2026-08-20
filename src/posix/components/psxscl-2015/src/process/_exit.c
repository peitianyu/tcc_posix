/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_impl.h"
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_exit.h"
#include "psx_debug.h"
#include "psx.h"

__psx_api
void __sys_exit(int status)
{
	__psx_exit(status);
}

int32_t __exit_group_dbg_status = 0x77771001;

__psx_api
void __sys_exit_group(int status)
{
	__exit_group_dbg_status = status;
	__ntapi->vms_client_disconnect(hport_vms);
	__ntapi->zw_terminate_process(NT_CURRENT_PROCESS_HANDLE,status);
	return;
}
