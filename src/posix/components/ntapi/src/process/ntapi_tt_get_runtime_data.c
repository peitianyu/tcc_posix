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

#if defined (__NT32)
static wchar16_t runtime_arg[12] = {
	' ','-','r',' ',
	'i','n','t','e','g','r','a','l'};
#elif defined (__NT64)
static wchar16_t runtime_arg[20] = {
	' ','-','r',' ',
	'i','n','t','e','g','r','a','l',
	'-','r','u','n','t','i','m','e'};
#endif

int32_t __stdcall __ntapi_tt_get_runtime_data(
	__out		nt_runtime_data **	rtdata,
	__in		wchar16_t **		argv)
{
	int32_t				status;
	nt_process_parameters *		process_params;
	nt_cmd_option_meta_utf16	cmd_opt_meta;
	nt_runtime_data			buffer;
	nt_runtime_data *		prtdata;
	ntapi_internals *		__internals;

	/* init */
	__internals = __ntapi_internals();

	/* once? */
	if (__internals->rtdata) {
		*rtdata = __internals->rtdata;
		return NT_STATUS_SUCCESS;
	}

	if (!(argv = argv ? argv : __internals->ntapi_img_sec_bss->argv_envp_array))
		return NT_STATUS_INVALID_PARAMETER_2;

	/* integral process? */
	if ((status = __ntapi->tt_get_short_option_meta_utf16(
			__ntapi->tt_crc32_table(),
			'r',
			argv,
			&cmd_opt_meta)))
		return status;

	else if (argv[3])
		status = NT_STATUS_INVALID_PARAMETER_MIX;

	if ((status = __ntapi->tt_hex_utf16_to_uintptr(
			cmd_opt_meta.value,
			(uintptr_t *)&prtdata)))
		return status;

	if ((status = __ntapi->zw_read_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			prtdata,
			(char *)&buffer,
			sizeof(buffer),0)))
		return status;

	/* avoid confusion :-) */
	process_params = ((nt_peb *)pe_get_peb_address())->process_params;

	__ntapi->tt_memcpy_utf16(
		(wchar16_t *)pe_va_from_rva(
			process_params->command_line.buffer,
			process_params->command_line.strlen - sizeof(runtime_arg)),
		runtime_arg,
		sizeof(runtime_arg));

	*rtdata = prtdata;

	return NT_STATUS_SUCCESS;
}
