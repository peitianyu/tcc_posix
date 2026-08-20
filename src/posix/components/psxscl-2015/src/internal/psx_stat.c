/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_stat.h"
#include "psx_ofd.h"
#include "psx_access.h"
#include "psx_time.h"
#include "psx_impl.h"
#include "psx.h"

int32_t __fastcall __psx_stat(struct __psx_tlca * tlca, struct __ofd * ofd, struct __stat * xstat)
{
	int32_t		status;
	nt_stat		nstat;

	if ((status = __ntapi->tt_stat(
			ofd->info.hfile,0,0,
			&nstat,tlca->buffer,(uint32_t)tlca->buflen,
			0,0)))
		return status;

	xstat->st_dev	= nstat.dev_name_hash;
	xstat->st_ino	= nstat.fii.index_number.quad;
	xstat->st_nlink = nstat.fsi.number_of_links;

	__psx_access_convert_native_to_posix(
		nstat.facci.access_flags,
		&xstat->st_mode);

	if (ofd->info.fdtype == PSX_FD_OS_FS_DIR)
		xstat->st_mode |= S_IFDIR;
	else if (ofd->info.fdtype == PSX_FD_OS_FS_FILE)
		xstat->st_mode |= S_IFREG;
	else if (ofd->info.fdtype == PSX_FD_OS_PIPE)
		xstat->st_mode |= S_IFIFO;

	xstat->st_uid	  = 0;
	xstat->st_gid	  = 0;
	xstat->__labi	  = 0;
	xstat->st_rdev	  = nstat.dev_name_hash;
	xstat->st_size	  = nstat.fsi.end_of_file.quad;
	xstat->st_blksize = nstat.fsi.allocation_size.quad;
	xstat->st_blocks	  = xstat->st_blksize
				? xstat->st_size / xstat->st_blksize + 1
				: 0;

	__psx_time_convert_native_to_timespec(nstat.fbi.last_access_time,&xstat->st_atim);
	__psx_time_convert_native_to_timespec(nstat.fbi.last_write_time,&xstat->st_mtim);
	__psx_time_convert_native_to_timespec(nstat.fbi.change_time,&xstat->st_ctim);
	__psx_time_convert_native_to_timespec(nstat.fbi.creation_time,&xstat->st_birthtime);

	return NT_STATUS_SUCCESS;
}
