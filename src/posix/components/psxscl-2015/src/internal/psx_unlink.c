/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_ofd.h"
#include "psx_unlink.h"
#include "psx.h"

int32_t __fastcall __psx_unlink(struct __ofd * ofd, uint32_t flags)
{
	int32_t  status;
	nt_fdi   fdi = {1};
	nt_iosb  iosb;
	nt_ftagi ftagi;

	if ((status = __psx_roattr_clear(ofd,&ftagi)))
		return status;

	status = __ntapi->zw_set_information_file(
		ofd->info.hfile,
		&iosb,&fdi,sizeof(fdi),
		NT_FILE_DISPOSITION_INFORMATION);

	__psx_roattr_restore(ofd,&ftagi);
	return status;
}
