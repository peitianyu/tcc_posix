/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_string.h>
#include <ntapi/nt_tty.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall	__ntapi_ipc_create_pipe_v2(
	__out		void **			hpipe_read,
	__out		void **			hpipe_write,
	__in		uint32_t		advisory_buffer_size	__optional)
{
	int32_t			status;

	void *			hdevpipes;
	void *			hwrite;
	void *			hread;

	nt_object_attributes	oa;
	nt_io_status_block	iosb;
	nt_sqos			sqos;
	nt_unicode_string	nt_name;
	nt_large_integer	timeout;

	const wchar16_t pipe_dir[] =  {
		'\\','D','e','v','i','c','e',
		'\\','N','a','m','e','d','P','i','p','e','\\',0
	};

	/* nt_name: pipe device directory */
	nt_name.strlen = (uint16_t)(sizeof(pipe_dir) - sizeof(wchar16_t));
	nt_name.maxlen = 0;
	nt_name.buffer = (uint16_t *)pipe_dir;

	/* init security structure */
	sqos.length 			= sizeof(sqos);
	sqos.impersonation_level	= NT_SECURITY_IMPERSONATION;
	sqos.context_tracking_mode	= NT_SECURITY_TRACKING_DYNAMIC;
	sqos.effective_only		= 1;

	/* oa */
	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &nt_name;
	oa.obj_attr	= NT_OBJ_CASE_INSENSITIVE | NT_OBJ_INHERIT;
	oa.sec_desc	= (nt_security_descriptor *)0;
	oa.sec_qos	= &sqos;

	status = __ntapi->zw_open_file(
		&hdevpipes,
		NT_GENERIC_READ | NT_SEC_SYNCHRONIZE,
		&oa,
		&iosb,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		NT_FILE_DIRECTORY_FILE);

	if (status != NT_STATUS_SUCCESS)
		return status;

	timeout.ihigh = 0xffffffff;
	timeout.ulow  = 0x0;

	oa.root_dir = hdevpipes;

	nt_name.strlen=0;
	nt_name.buffer = (uint16_t *)0;

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
		0X2000,
		0x2000,
		&timeout);

	if (status != NT_STATUS_SUCCESS) {
		__ntapi->zw_close(hdevpipes);
		return status;
	}

	/* the pipe is now our root directory */
	oa.root_dir = hread;

	status = __ntapi->zw_open_file(
		&hwrite,
		NT_GENERIC_WRITE | NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES,
		&oa,
		&iosb,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		NT_FILE_WRITE_THROUGH | NT_FILE_ASYNCHRONOUS_IO | NT_FILE_NON_DIRECTORY_FILE);

	if (status != NT_STATUS_SUCCESS) {
		__ntapi->zw_close(hdevpipes);
		__ntapi->zw_close(hread);
		return status;
	}

	*hpipe_read  = hread;
	*hpipe_write = hwrite;

	return status;
}
