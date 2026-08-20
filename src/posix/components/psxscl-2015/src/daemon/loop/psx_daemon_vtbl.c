/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_daemon.h"
#include "psx_tlca.h"
#include "psx.h"

psx_daemon_routine * psx_daemon_vtbl[PSX_DAEMON_OPCODE_CAP - PSX_DAEMON_OPCODE_BASE] = {
	__psx_daemon_connect,
	0,
	0,
	0,
	0,
	0,
	(psx_daemon_routine *)__psx_daemon_sigsend,
	0,
	0,
	(psx_daemon_routine *)__psx_daemon_setitimer,
	0,
	(psx_daemon_routine *)__psx_daemon_threadexit
};
