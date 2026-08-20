/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx.h"

__psx_api
gid_t __sys_getegid(void)
{
	return __psx.__egid;
}

__psx_api
uid_t __sys_geteuid(void)
{
	return __psx.__euid;
}

__psx_api
gid_t __sys_getgid(void)
{
	return __psx.__gid;
}

__psx_api
uid_t __sys_getuid(void)
{
	return __psx.__uid;
}

__psx_api
intptr_t __sys_setgid(gid_t gid)
{
	return __psx.__gid = gid;
	return 0;
}

__psx_api
intptr_t __sys_setuid(uid_t uid)
{
	return __psx.__uid = uid;
	return 0;
}
