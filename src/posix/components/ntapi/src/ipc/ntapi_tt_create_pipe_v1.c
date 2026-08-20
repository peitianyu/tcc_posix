/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_string.h>
#include <ntapi/nt_atomic.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


typedef struct __attr_ptr_size_aligned__ _nt_tty_pipe_name {
	wchar16_t	pipe_dir[8];
	wchar16_t	back_slash;
	wchar16_t	key_1st[8];
	wchar16_t	uscore_1st;
	wchar16_t	key_2nd[8];
	wchar16_t	uscore_2nd;
	wchar16_t	key_3rd[8];
	wchar16_t	uscore_3rd;
	wchar16_t	key_4th[8];
	wchar16_t	uscore_4th;
	wchar16_t	key_5th[8];
	wchar16_t	uscore_5th;
	wchar16_t	key_6th[8];
	wchar16_t	null_termination;
} nt_tty_pipe_name;


int32_t __stdcall	__ntapi_ipc_create_pipe_v1(
	__out		void **			hpipe_read,
	__out		void **			hpipe_write,
	__in		uint32_t		advisory_buffer_size	__optional)
{
	int32_t			status;

	void *				hread;
	void *				hwrite;

	nt_object_attributes		oa;
	nt_io_status_block		iosb;
	nt_unicode_string		nt_name;
	nt_security_quality_of_service	sqos;
	nt_large_integer		timeout;
	intptr_t *			counter;

	nt_tty_pipe_name pipe_name =  {
		{'\\','?','?','\\','p','i','p','e'},
		'\\',
		{0},'_',
		{0},'_',
		{0},'_',
		{0},'_',
		{0},'_',
		{0},
		0
	};

	/* pipe_count  */
	counter = (intptr_t *)&__ntapi_internals()->v1_pipe_counter;
	at_locked_inc(counter);

	/* get system time */
	status = __ntapi->zw_query_system_time(&timeout);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* pipe name (no anonymous pipe prior to vista) */
	__ntapi->tt_uint32_to_hex_utf16(	pe_get_current_process_id(),pipe_name.key_1st);
	__ntapi->tt_uint32_to_hex_utf16(	pe_get_current_thread_id(),pipe_name.key_2nd);

	__ntapi->tt_uint32_to_hex_utf16(	timeout.ihigh + (uint32_t)*counter,pipe_name.key_3rd);
	__ntapi->tt_uint32_to_hex_utf16(timeout.ulow + (uint32_t)*counter,pipe_name.key_4th);

	__ntapi->tt_uint32_to_hex_utf16(
		__ntapi->tt_buffer_crc32(0,(char *)&pipe_name,sizeof(pipe_name)),
		pipe_name.key_5th);

	__ntapi->tt_uint32_to_hex_utf16(
		__ntapi->tt_buffer_crc32(0,(char *)&pipe_name,sizeof(pipe_name)),
		pipe_name.key_6th);

	__ntapi->tt_uint32_to_hex_utf16(
		__ntapi->tt_buffer_crc32(0,(char *)&pipe_name,sizeof(pipe_name)),
		pipe_name.key_1st);

	__ntapi->tt_uint32_to_hex_utf16(
		__ntapi->tt_buffer_crc32(0,(char *)&pipe_name,sizeof(pipe_name)),
		pipe_name.key_2nd);

	__ntapi->tt_uint32_to_hex_utf16(
		__ntapi->tt_buffer_crc32(0,(char *)&pipe_name,sizeof(pipe_name)),
		pipe_name.key_3rd);

	__ntapi->tt_uint32_to_hex_utf16(
		__ntapi->tt_buffer_crc32(0,(char *)&pipe_name,sizeof(pipe_name)),
		pipe_name.key_4th);

	/* nt_name */
	nt_name.strlen = (uint16_t)(sizeof(pipe_name) - sizeof(wchar16_t));
	nt_name.maxlen = (uint16_t)(sizeof(pipe_name));
	nt_name.buffer = (uint16_t *)&pipe_name;

	/* init security structure */
	sqos.length 			= sizeof(sqos);
	sqos.impersonation_level	= NT_SECURITY_IMPERSONATION;
	sqos.context_tracking_mode	= NT_SECURITY_TRACKING_DYNAMIC;
	sqos.effective_only		= 1;

	/* oa */
	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &nt_name;
	oa.obj_attr	= 0x0;
	oa.sec_desc	= (nt_security_descriptor *)0;
	oa.sec_qos	= &sqos;

	timeout.ihigh = 0xffffffff;
	timeout.ulow  = 0x0;

	/* the reading end */
	status = __ntapi->zw_create_named_pipe_file(
		&hread,
		NT_GENERIC_READ | NT_SEC_SYNCHRONIZE | NT_FILE_WRITE_ATTRIBUTES,
		&oa,
		&iosb,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		NT_FILE_CREATE,
		NT_FILE_ASYNCHRONOUS_IO,
		0,
		0,
		0,
		1,
		0x2000,
		0x2000,
		&timeout);

	if (status != NT_STATUS_SUCCESS) {
		return status;
	}

	/* the writing end(s) */
	status = __ntapi->zw_open_file(
		&hwrite,
		NT_GENERIC_WRITE | NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES,
		&oa,
		&iosb,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		NT_FILE_WRITE_THROUGH | NT_FILE_ASYNCHRONOUS_IO | NT_FILE_NON_DIRECTORY_FILE);

	if (status != NT_STATUS_SUCCESS) {
		__ntapi->zw_close(hread);
		return status;
	}

	*hpipe_read  = hread;
	*hpipe_write = hwrite;

	return status;
}
