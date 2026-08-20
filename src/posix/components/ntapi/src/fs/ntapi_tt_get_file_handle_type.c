/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_file.h>
#include "ntapi_impl.h"

typedef int __stdcall winapi_get_console_mode(void * handle, uint32_t * mode);

int32_t __stdcall __ntapi_tt_get_file_handle_type(
	__in	void *		handle,
	__out	int32_t *	type)
{
	int32_t				status;
	uint32_t			info;
	nt_iosb				iosb;
	nt_fsssi			fsssi;
	nt_file_directory_information	fdi;
	nt_file_pipe_information	fpi;
	nt_object_basic_information	obi;

	void *				hkernel32;
	char				str_get_con_mode[32] = "GetConsoleMode";
	winapi_get_console_mode *	pfn_get_con_mode;

	/* validation */
	if (!handle) return NT_STATUS_INVALID_HANDLE;

	/* file-system directory? */
	if (!(status = __ntapi->zw_query_information_file(
			handle,
			&iosb,&fdi,sizeof(fdi),
			NT_FILE_DIRECTORY_INFORMATION))) {
		*type = NT_FILE_TYPE_DIRECTORY;
		return 0;
	}

	/* file-system file? */
	if (!(status = __ntapi->zw_query_volume_information_file(
			handle,
			&iosb,&fsssi,sizeof(fsssi),
			NT_FILE_FS_SECTOR_SIZE_INFORMATION))) {
		*type = NT_FILE_TYPE_FILE;
		return 0;
	}

	/* pipe? */
	if (!(status = __ntapi->zw_query_information_file(
			handle,
			&iosb,&fpi,sizeof(fpi),
			NT_FILE_PIPE_INFORMATION))) {
		*type = NT_FILE_TYPE_PIPE;
		return 0;
	}


	/* csrss? */
	if (!(hkernel32 = pe_get_kernel32_module_handle()))
		return NT_STATUS_DLL_INIT_FAILED;
	else if (!(pfn_get_con_mode = (winapi_get_console_mode *)pe_get_procedure_address(
			hkernel32,str_get_con_mode)))
		return NT_STATUS_DLL_INIT_FAILED;


	/* (console functions return non-zero on success) */
	if ((pfn_get_con_mode(handle,&info))) {
		*type = NT_FILE_TYPE_CSRSS;
		return 0;
	}

	/* invalid handle? */
	if ((status = __ntapi->zw_query_object(
			handle,NT_OBJECT_BASIC_INFORMATION,
			&obi,sizeof(obi),&info)))
		return status;

	/* unknown object */
	*type = NT_FILE_TYPE_UNKNOWN;
	return NT_STATUS_SUCCESS;
}
