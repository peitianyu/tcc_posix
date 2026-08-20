/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_fsctl.h>
#include <ntapi/nt_mount.h>
#include <ntapi/nt_istat.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_istat(
	__in	void *			hfile,
	__in	void *			hroot	__optional,
	__in	nt_unicode_string *	path,
	__out	nt_istat *		istat,
	__out	uintptr_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	uint32_t		open_options,
	__in	uint32_t		flags)
{
	int32_t			status;

	nt_oa			oa;
	nt_iosb			iosb;
	nt_unicode_string *	sdev;
	uint32_t		hash;
	wchar16_t *		wch;
	wchar16_t *		wch_mark;

	/* validaton */
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

		istat->flags_out = NT_STAT_NEW_HANDLE;
	}

	istat->hfile    = hfile;
	istat->flags_in = flags;

	/* file index number */
	status = __ntapi->zw_query_information_file(
		hfile,
		&iosb,
		&istat->fii,
		sizeof(istat->fii),
		NT_FILE_INTERNAL_INFORMATION);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* attributes & reparse tag information */
	status = __ntapi->zw_query_information_file(
		hfile,
		&iosb,
		&istat->ftagi,
		sizeof(istat->ftagi),
		NT_FILE_ATTRIBUTE_TAG_INFORMATION);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* TODO: consolidate with statfs */
	/* system-unique device name */
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

	if (sdev->strlen < __DEVICE_PATH_PREFIX_LEN)
		return NT_STATUS_INVALID_HANDLE;

	hash = __ntapi->tt_buffer_crc32(
		0,
		sdev->buffer,
		__DEVICE_PATH_PREFIX_LEN);

	if (hash != __DEVICE_PATH_PREFIX_HASH)
		return NT_STATUS_INVALID_HANDLE;

	wch_mark = sdev->buffer + __DEVICE_PATH_PREFIX_LEN/sizeof(wchar16_t);
	wch = wch_mark;
	while (*wch != '\\') wch++;
	istat->dev_name_strlen = (uint16_t)((wch - sdev->buffer) * sizeof(uint16_t));

	istat->dev_name_hash = __ntapi->tt_buffer_crc32(
		hash,
		wch_mark,
		(uintptr_t)wch - (uintptr_t)wch_mark);

	return status;
}


int32_t __stdcall __ntapi_tt_validate_fs_handle(
	__in	void *		hfile,
	__in	uint32_t	dev_name_hash,
	__in	nt_fii		fii,
	__out	uintptr_t *	buffer,
	__in	uint32_t	buffer_size)
{
	int32_t		status;
	nt_istat	istat;

	status = __ntapi->tt_istat(
		hfile,
		(void *)0,
		(nt_unicode_string *)0,
		&istat,
		buffer,
		buffer_size,
		0,
		NT_ISTAT_DEFAULT);

	if (status) return status;

	if (istat.fii.index_number.quad != fii.index_number.quad)
		return NT_STATUS_CONTEXT_MISMATCH;
	else if (istat.dev_name_hash != dev_name_hash)
		return NT_STATUS_CONTEXT_MISMATCH;

	return NT_STATUS_SUCCESS;
}
