/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_socket.h"
#include "psx_tlca.h"
#include "psx_impl.h"
#include "psx.h"

__psx_api
ssize_t __sys_sendmsg(int socket, const struct __msghdr * msg, int flags)
{
	/* todo: integrate from internal tree */
	return -ENOSYS;
}
