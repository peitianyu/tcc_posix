/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_fsctl.h>
#include <ntapi/nt_mount.h>
#include <ntapi/nt_stat.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_stat(
	__in	void *			hfile,
	__in	void *			hroot	__optional,
	__in	nt_unicode_string *	path,
	__out	nt_stat *		stat,
	__out	uintptr_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	uint32_t		open_options,
	__in	uint32_t		flags)
{
	int32_t			status;
	nt_oa			oa;
	nt_iosb			iosb;
	nt_unicode_string *	sdev;
	nt_fai *		fai;

	/* validation */
	if (!hfile && !path)
		return NT_STATUS_INVALID_HANDLE;

	/* hfile */
	if (!hfile) {
		/* oa */
		oa.len = sizeof(nt_oa);
		oa.root_dir = hroot;
		oa.obj_name = path;
		oa.obj_attr = 0;
		oa.sec_desc = 0;
		oa.sec_qos = 0;

		/* open file/folder */
		status = __ntapi->zw_open_file(
			&hfile,
			NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES | NT_FILE_READ_ACCESS,
			&oa,
			&iosb,
			NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
			open_options | NT_FILE_SYNCHRONOUS_IO_ALERT);

		if (status != NT_STATUS_SUCCESS)
			return status;

		stat->flags_out = NT_STAT_NEW_HANDLE;
	}

	stat->hfile    = hfile;
	stat->flags_in = flags;

	/* system-unique device name */
	status = __ntapi->zw_query_information_file(
		hfile,
		&iosb,
		buffer,
		buffer_size,
		NT_FILE_ALL_INFORMATION);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* copy file info minus name */
	fai = (nt_fai *)buffer;
	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)stat,
		(uintptr_t *)fai,
		((size_t)(&((nt_fai *)0)->name_info)));

	/* record the file name length, but do not hash */
	stat->file_name_length = fai->name_info.file_name_length;
	stat->file_name_hash   = 0;


	/* file system size information */
	status = __ntapi->zw_query_volume_information_file(
		hfile,
		&iosb,
		&(stat->fssi),
		sizeof(stat->fssi),
		NT_FILE_FS_SIZE_INFORMATION);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* system-unique device name (simpler than statfs) */
	iosb.info = 0;
	status = __ntapi->zw_query_object(
		hfile,
		NT_OBJECT_NAME_INFORMATION,
		buffer,
		buffer_size,
		(uint32_t *)&iosb.info);

	if (status != NT_STATUS_SUCCESS)
		return status;

	sdev = (nt_unicode_string *)buffer;
	stat->dev_name_strlen = sdev->strlen - (uint16_t)stat->file_name_length;

	stat->dev_name_hash = __ntapi->tt_buffer_crc32(
		0,
		sdev->buffer,
		stat->dev_name_strlen);

	if (flags & NT_STAT_DEV_NAME_COPY) {
		if (stat->dev_name_maxlen < sdev->strlen)
			/* does not justify failure */
			*stat->dev_name = 0;
		else
			__ntapi->tt_memcpy_utf16(
				(wchar16_t *)stat->dev_name,
				(wchar16_t *)sdev->buffer,
				stat->dev_name_strlen);
	} else
		*stat->dev_name = 0;

	return status;
}
