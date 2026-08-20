/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_memory.h>
#include <ntapi/nt_process.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

typedef struct _nt_process_basic_information nt_pbi;

int32_t __stdcall __ntapi_tt_create_remote_runtime_data(
	__in		void *			hprocess,
	__in_out	nt_runtime_data_block *	rtblock)
{
	int32_t			status;

	size_t			bytes_written;
	nt_pbi			rpbi;
	nt_process_parameters *	rprocess_params;
	nt_unicode_string	rcmd_line;
	uint32_t		runtime_arg_hash;
	nt_runtime_data *	rtdata;
	void *			srv_ready;

	#if defined (__NT32)
	wchar16_t		runtime_arg[8] = {
		'i','n','t','e','g','r','a','l'};
	#elif defined (__NT64)
	wchar16_t		runtime_arg[16] = {
		'i','n','t','e','g','r','a','l',
		'-','r','u','n','t','i','m','e'};
	#endif

	/* validation */
	if (!hprocess)
		return NT_STATUS_INVALID_PARAMETER_1;
	else if (!rtblock)
		return NT_STATUS_INVALID_PARAMETER_2;
	else if (!rtblock->addr)
		return NT_STATUS_INVALID_PARAMETER_2;
	else if (!rtblock->size)
		return NT_STATUS_INVALID_PARAMETER_2;

	runtime_arg_hash = __ntapi->tt_buffer_crc32(
		0,
		(char *)runtime_arg,
		sizeof(runtime_arg));

	/* obtain process information */
	status = __ntapi->zw_query_information_process(
		hprocess,
		NT_PROCESS_BASIC_INFORMATION,
		(void *)&rpbi,
		sizeof(nt_process_basic_information),
		0);

	if (status != NT_STATUS_SUCCESS)
		return status;

	status = __ntapi->zw_read_virtual_memory(
		hprocess,
		pe_va_from_rva(
			rpbi.peb_base_address,
			(uintptr_t)&(((nt_peb *)0)->process_params)),
		(char *)&rprocess_params,
		sizeof(uintptr_t),
		&bytes_written);

	if (status != NT_STATUS_SUCCESS)
		return status;

	status = __ntapi->zw_read_virtual_memory(
		hprocess,
		&rprocess_params->command_line,
		(char *)&rcmd_line,
		sizeof(nt_unicode_string),
		&bytes_written);

	if (status != NT_STATUS_SUCCESS)
		return status;

	if (rcmd_line.buffer == 0)
		return NT_STATUS_BUFFER_TOO_SMALL;
	else if (rcmd_line.strlen < sizeof(runtime_arg) + 4*sizeof(wchar16_t))
		return NT_STATUS_INVALID_USER_BUFFER;

	status = __ntapi->zw_read_virtual_memory(
		hprocess,
		pe_va_from_rva(
			rcmd_line.buffer,
			rcmd_line.strlen - sizeof(runtime_arg)),
		(char *)&runtime_arg,
		sizeof(runtime_arg),
		&bytes_written);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* verify remote process compatibility */
	runtime_arg_hash ^= __ntapi->tt_buffer_crc32(
		0,
		(char *)runtime_arg,
		sizeof(runtime_arg));

	if (runtime_arg_hash)
		return NT_STATUS_INVALID_SIGNATURE;

	/* remote block */
	rtblock->remote_size = rtblock->size;
	status = __ntapi->zw_allocate_virtual_memory(
		hprocess,
		&rtblock->remote_addr,
		0,
		&rtblock->remote_size,
		NT_MEM_RESERVE | NT_MEM_COMMIT,
		NT_PAGE_READWRITE);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* session handles */
	if (rtblock->flags & NT_RUNTIME_DATA_DUPLICATE_SESSION_HANDLES) {
		rtdata = (nt_runtime_data *)rtblock->addr;
		srv_ready = rtdata->srv_ready;

		status = __ntapi->zw_duplicate_object(
			NT_CURRENT_PROCESS_HANDLE,
			srv_ready,
			hprocess,
			&rtdata->srv_ready,
			0,0,NT_DUPLICATE_SAME_ATTRIBUTES | NT_DUPLICATE_SAME_ACCESS);
		if (status) return status;
	} else
		srv_ready = 0;

	/* copy local block to remote process */
	status = __ntapi->zw_write_virtual_memory(
		hprocess,
		rtblock->remote_addr,
		(char *)rtblock->addr,
		rtblock->size,
		&bytes_written);

	/* restore rtdata */
	if (srv_ready)
		rtdata->srv_ready = srv_ready;

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* runtime_arg */
	__ntapi->tt_uintptr_to_hex_utf16(
		(uintptr_t)rtblock->remote_addr,
		runtime_arg);

	/* update remote runtime arg */
	status = __ntapi->zw_write_virtual_memory(
		hprocess,
		pe_va_from_rva(
			rcmd_line.buffer,
			rcmd_line.strlen - sizeof(runtime_arg)),
		(char *)&runtime_arg,
		sizeof(runtime_arg),
		&bytes_written);

	if (status)
		__ntapi->zw_free_virtual_memory(
			hprocess,
			&rtblock->remote_addr,
			&rtblock->remote_size,
			NT_MEM_RELEASE);

	return status;
}
