/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_file.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_open_physical_parent_directory(
	__out	void **		hparent,
	__in	void *		hdir,
	__out	uintptr_t *	buffer,
	__in	uint32_t	buffer_size,
	__in	uint32_t	desired_access,
	__in	uint32_t	open_options,
	__out	int32_t *	type)
{
	int32_t				status;
	nt_oa				oa;
	nt_iosb				iosb;
	wchar16_t *			wch;
	nt_unicode_string *		path;
	uint32_t			len;

	path = (nt_unicode_string *)buffer;

	if ((status = __ntapi->zw_query_object(
			hdir,
			NT_OBJECT_NAME_INFORMATION,
			path,
			buffer_size,
			&len)))
		return status;
	else if (len == sizeof(nt_unicode_string))
		return NT_STATUS_BAD_FILE_TYPE;

	wch = path->buffer + (path->strlen / sizeof(uint16_t));
	while ((--wch >= path->buffer) && (*wch != '\\'));

	if (wch == path->buffer )
		return NT_STATUS_MORE_PROCESSING_REQUIRED;

	path->strlen = sizeof(uint16_t) * (uint16_t)(wch-path->buffer);
	path->maxlen = 0;

	/* oa */
	oa.len = sizeof(nt_oa);
	oa.root_dir = 0;
	oa.obj_name = path;
	oa.obj_attr = 0;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* default access */
	desired_access = desired_access
		? desired_access
		: NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES | NT_FILE_READ_ACCESS;

	/* open parent directory */
	return __ntapi->zw_open_file(
		hparent,
		desired_access,
		&oa,
		&iosb,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		open_options | NT_FILE_DIRECTORY_FILE);
}
