/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_limits.h"
#include "psx_daemon.h"
#include "psx.h"

int32_t __stdcall __psx_daemon_connect(struct __port_msg * msg)
{
	void * hport = 0;

	msg->msginfo.key = 0;

	__ntapi->zw_accept_connect_port(
		&hport,
		msg->header.client_id.process_id,
		&msg->header,
		NT_LPC_ACCEPT_CONNECTION,0,0);

	return __ntapi->zw_complete_connect_port(hport);
}
