/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include "psx_systypes.h"
#include "psx_stat.h"
#include "psx_access.h"
#include "psx.h"

void __psx_access_convert_native_to_posix(uint32_t naccess, mode_t * xaccess)
{
	/* todo (tedious only) */
	*xaccess = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
}
