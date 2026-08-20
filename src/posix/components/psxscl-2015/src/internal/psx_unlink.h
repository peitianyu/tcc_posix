/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_UNLINK_H_
#define _PSX_UNLINK_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_ofd.h"
#include "psx.h"

int32_t __fastcall __psx_unlink(struct __ofd * ofd, uint32_t flags);

static __inline__ int32_t __psx_roattr_clear(struct __ofd * ofd, nt_ftagi * ftagi)
{
	int32_t	 status;
	nt_ftagi nattr;
	nt_iosb  iosb;

	if ((status = __ntapi->zw_query_information_file(
			ofd->info.hfile,
			&iosb,ftagi,sizeof(*ftagi),
			NT_FILE_ATTRIBUTE_TAG_INFORMATION)))
		return status;

	if (ftagi->file_attr & NT_FILE_ATTRIBUTE_READONLY) {
		nattr.file_attr = ftagi->file_attr & ~NT_FILE_ATTRIBUTE_READONLY;

		status = __ntapi->zw_set_information_file(
			ofd->info.hfile,
			&iosb,&nattr,sizeof(nattr),
			NT_FILE_ATTRIBUTE_TAG_INFORMATION);
	}

	return status;
}

static __inline__ int32_t __psx_roattr_restore(struct __ofd * ofd, nt_ftagi * ftagi)
{
	nt_iosb iosb;

	if (ftagi->file_attr & NT_FILE_ATTRIBUTE_READONLY)
		return __ntapi->zw_set_information_file(
			ofd->info.hfile,
			&iosb,ftagi,sizeof(*ftagi),
			NT_FILE_ATTRIBUTE_TAG_INFORMATION);
	else
		return NT_STATUS_SUCCESS;
}

#endif
