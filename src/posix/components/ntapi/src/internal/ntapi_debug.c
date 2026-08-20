/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#ifdef __DEBUG

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_file.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

char dbg_buf[0x1000];

ssize_t __cdecl __dbg_write(
	__in	void *		hfile,
	__in	const void *	buf,
	__in	size_t		bytes)
{
	nt_iosb		iosb;
	int32_t		status;

	status = __ntapi->zw_write_file(
		hfile,
		(void *)0,
		(nt_io_apc_routine *)0,
		(void *)0,
		&iosb,
		(void *)buf,
		(uint32_t)bytes,
		(nt_large_integer *)0,
		(uint32_t *)0);

	if (status == NT_STATUS_SUCCESS)
		return iosb.info;
	else
		return -1;
}


int32_t __cdecl __dbg_fn_call(
	__in	void *			hfile		__optional,
	__in	char *			fn_caller_name,
	__in	void *			fn_callee_addr,
	__in	uintptr_t		fn_ret,
	__in	ntapi_dbg_write*	pfn_dbg_write	__optional,
	__in	char *			source		__optional,
	__in	int			line		__optional)
{
	struct pe_ldr_tbl_entry *	image_meta;
	void *				image_base;
	char *				fn_name;
	size_t				bytes;
	char				dbg_buf[256];

	if (!pfn_dbg_write)
		pfn_dbg_write = __dbg_write;

	image_meta = pe_get_symbol_module_info(fn_callee_addr);
	fn_name    = (char *)0;

	if (image_meta)
		image_base = image_meta->dll_base;
	else
		image_base = (void *)0;


	if (image_base)
		fn_name = pe_get_symbol_name(
			image_base,
			fn_callee_addr);

	if (!fn_name)
		fn_name = pe_get_import_symbol_info(
			fn_callee_addr,
			(void **)0,
			(char **)0,
			&image_meta);

	if (source && fn_name)
		bytes = __ntapi->sprintf(
				dbg_buf,
				"%s: (%s:%d):\n"
				"--> %s returned 0x%08x\n\n",
				fn_caller_name, source, line, fn_name, fn_ret);
	else if (fn_name)
		bytes = __ntapi->sprintf(
				dbg_buf,
				"%s: %s returned 0x%08x\n\n",
				fn_caller_name, fn_name, fn_ret);
	else if (source)
		bytes = __ntapi->sprintf(
				dbg_buf,
				"%s: (%s:%d):\n"
				"--> calling 0x%08x returned 0x%08x\n\n",
				fn_caller_name, source, line, fn_callee_addr, fn_ret);
	else
		bytes = __ntapi->sprintf(
				dbg_buf,
				"%s: calling 0x%08x returned 0x%08x\n\n",
				fn_caller_name, fn_callee_addr, fn_ret);

	if (bytes) {
		bytes = __ntapi->strlen(dbg_buf);

		if (bytes == pfn_dbg_write(hfile,dbg_buf,bytes))
			return NT_STATUS_SUCCESS;
		else
			return NT_STATUS_UNSUCCESSFUL;
	} else
		return NT_STATUS_UNSUCCESSFUL;
}


int32_t __cdecl __dbg_msg(
	__in	void *			hfile		__optional,
	__in	char *			source		__optional,
	__in	int			line		__optional,
	__in	char *			fn_caller_name,
	__in	char *			fmt,
	__in	uintptr_t		arg1,
	__in	uintptr_t		arg2,
	__in	uintptr_t		arg3,
	__in	uintptr_t		arg4,
	__in	uintptr_t		arg5,
	__in	uintptr_t		arg6,
	__in	ntapi_dbg_write*	pfn_dbg_write	__optional)
{
	char *		buffer;
	size_t		bytes;

	if (!pfn_dbg_write)
		pfn_dbg_write = __dbg_write;

	bytes  = 0;
	buffer = dbg_buf;

	if (source)
		bytes = __ntapi->sprintf(
				buffer,
				"%s: (%s:%d):\n--> ",
				fn_caller_name,source,line);
	else if (fn_caller_name)
		bytes = __ntapi->sprintf(
				buffer,
				"%s: ",
				fn_caller_name);
	else
		dbg_buf[0] = '\0';

	if (bytes >= 0)
		buffer += __ntapi->strlen(dbg_buf);
	else
		return NT_STATUS_UNSUCCESSFUL;

	bytes = __ntapi->sprintf(buffer,fmt,arg1,arg2,arg3,arg4,arg5,arg6);

	if (bytes) {
		bytes = __ntapi->strlen(dbg_buf);

		if (bytes == pfn_dbg_write(hfile,dbg_buf,bytes))
			return NT_STATUS_SUCCESS;
		else
			return NT_STATUS_UNSUCCESSFUL;
	} else
		return NT_STATUS_UNSUCCESSFUL;
}

#endif
