/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_sysinfo.h>
#include <ntapi/nt_memory.h>
#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_get_system_directory_native_path(
	__out	nt_mem_sec_name *	buffer,
	__in	uint32_t		buffer_size,
	__in	wchar16_t *		base_name,
	__in	uint32_t		base_name_size,
	__out	nt_unicode_string *	nt_path		__optional)
{
	int32_t			status;
	wchar16_t *		wch_src;
	wchar16_t *		wch_dst;
	wchar16_t *		wch_cap;
	size_t			maxlen_saved;
	size_t			info_size;

	/* validation */
	if (!buffer || !buffer_size)
		return NT_STATUS_BUFFER_TOO_SMALL;
	else if (base_name && !base_name_size)
		return NT_STATUS_INVALID_PARAMETER_MIX;

	/* init buffer */
	buffer->section_name.strlen = 0;
	buffer->section_name.maxlen = (uint16_t)(buffer_size - sizeof(nt_unicode_string));
	buffer->section_name.buffer = buffer->section_name_buffer;

	maxlen_saved = buffer->section_name.maxlen;
	info_size    = 0;

	status = __ntapi->zw_query_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		pe_get_ntdll_module_handle(),
		NT_MEMORY_SECTION_NAME,
		buffer,
		buffer_size,
		&info_size);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* find directory portion */
	wch_dst = buffer->section_name.buffer + (buffer->section_name.strlen / sizeof(wchar16_t));
	wch_dst--;

	while ((*wch_dst != '\\') && (wch_dst > buffer->section_name.buffer))
		wch_dst--;

	if (wch_dst == buffer->section_name.buffer)
		return NT_STATUS_INTERNAL_ERROR;

	/* base_name */
	if (base_name) {
		wch_dst++;
		wch_src	= base_name;
		wch_cap = (wchar16_t *)((uintptr_t)wch_dst + base_name_size);

		if ((uintptr_t)wch_cap - (uintptr_t)(buffer->section_name.buffer) > maxlen_saved)
			return NT_STATUS_BUFFER_TOO_SMALL;

		while (wch_dst < wch_cap) {
			*wch_dst = *wch_src;
			wch_dst++;
			wch_src++;
		}
	}

	/* null termination */
	*wch_dst = 0;

	/* nt_path */
	if (nt_path)
		__ntapi->rtl_init_unicode_string(
			nt_path,
			buffer->section_name.buffer);

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_get_system_directory_handle(
	__out	void **			hsysdir,
	__out	nt_mem_sec_name *	buffer		__optional,
	__in	uint32_t		buffer_size	__optional)
{
	int32_t			status;
	nt_oa			oa;
	nt_iosb			iosb;
	nt_unicode_string	path;
	char			_buffer[256];

	/* validation */
	if (!hsysdir)
		return NT_STATUS_INVALID_PARAMETER_1;
	else if (buffer_size && buffer_size < 0x20)
		return NT_STATUS_BUFFER_TOO_SMALL;

	/* buffer */
	if (!buffer) {
		buffer		= (nt_mem_sec_name *)_buffer;
		buffer_size	= sizeof(_buffer);
		__ntapi->tt_aligned_block_memset(buffer,0,sizeof(buffer));
	}

	/* sysdir path */
	status = __ntapi_tt_get_system_directory_native_path(
		buffer,
		buffer_size,
		(wchar16_t *)0,
		0,
		&path);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* oa */
	oa.len = sizeof(nt_oa);
	oa.root_dir = (void *)0;
	oa.obj_name = &path;
	oa.obj_attr = 0;
	oa.sec_desc = 0;
	oa.sec_qos = 0;

	/* open file/folder */
	status = __ntapi->zw_open_file(
		hsysdir,
		NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES | NT_FILE_READ_ACCESS,
		&oa,
		&iosb,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		NT_FILE_DIRECTORY_FILE | NT_FILE_SYNCHRONOUS_IO_ALERT);

	return status;
}


int32_t __stdcall __ntapi_tt_get_system_directory_dos_path(
	__in	void *			hsysdir		__optional,
	__out	wchar16_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	wchar16_t *		base_name,
	__in	uint32_t		base_name_size,
	__out	nt_unicode_string *	nt_path		__optional)
{
	int32_t			status;
	nt_statfs		statfs;
	wchar16_t *		wch;
	wchar16_t *		wch_src;
	wchar16_t *		wch_cap;
	nt_iosb			iosb;
	nt_fni *		fni;
	uint32_t		fni_length;

	/* validation */
	if (!buffer)
		return NT_STATUS_INVALID_PARAMETER_2;

	/* hsysdir */
	if (!hsysdir) {
		status = __ntapi_tt_get_system_directory_handle(
			&hsysdir,
			(nt_mem_sec_name *)buffer,
			buffer_size);

		if (status != NT_STATUS_SUCCESS)
			return status;
	}

	/* statfs */
	status = __ntapi->tt_statfs(
			hsysdir,
			(void *)0,
			(nt_unicode_string *)0,
			&statfs,
			(uintptr_t *)buffer,
			buffer_size,
			NT_STATFS_DOS_DRIVE_LETTER);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* dos path name (always shorter than the native path, so buffer_size must be ok) */
	wch = buffer;
	*wch = '\\'; wch++;
	*wch = '?';  wch++;
	*wch = '?';  wch++;
	*wch = '\\'; wch++;
	*wch = statfs.nt_drive_letter; wch++;
	*wch = ':';  wch++;

	/* alignment */
	fni  = (nt_fni *)((uintptr_t)buffer + 0x10);

	status = __ntapi->zw_query_information_file(
		hsysdir,
		&iosb,
		fni,
		buffer_size - 8 * sizeof(wchar16_t),
		NT_FILE_NAME_INFORMATION);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* fni->file_name_length: save */
	fni_length = fni->file_name_length;

	/* overwrite */
	wch_src  = fni->file_name;
	wch_cap  = (wchar16_t *)((uintptr_t)wch_src + fni_length);

	while (wch_src < wch_cap) {
		*wch = *wch_src;
		wch++;
		wch_src++;
	}

	/* ultimate path separator */
	*wch = '\\'; wch++;

	/* base_name */
	if (base_name) {
		wch_src	= base_name;
		wch_cap = (wchar16_t *)((uintptr_t)wch + base_name_size);

		if ((uintptr_t)wch_cap - (uintptr_t)buffer - sizeof(wchar16_t) > buffer_size)
			return NT_STATUS_BUFFER_TOO_SMALL;

		while (wch < wch_cap) {
			*wch = *wch_src;
			wch++;
			wch_src++;
		}
	}

	/* null termination */
	*wch = 0;

	/* nt_path */
	if (nt_path)
		__ntapi->rtl_init_unicode_string(
			nt_path,
			buffer);

	return NT_STATUS_SUCCESS;
}
