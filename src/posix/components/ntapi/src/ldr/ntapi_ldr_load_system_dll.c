/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_ldr.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t	__stdcall __ntapi_ldr_load_system_dll(
	__in	void *			hsysdir		__optional,
	__in	wchar16_t *		base_name,
	__in	uint32_t		base_name_size,
	__in	uint32_t *		image_flags	__optional,
	__out	void **			image_base)
{
	int32_t			status;
	nt_unicode_string	nt_image_name;
	uintptr_t		buffer[0x80];

	/* stack buffer */
	__ntapi->tt_aligned_block_memset(buffer,0,sizeof(buffer));

	status = __ntapi->tt_get_system_directory_dos_path(
		hsysdir,
		(wchar16_t *)buffer,
		sizeof(buffer),
		base_name,
		base_name_size,
		&nt_image_name);

	if (status != NT_STATUS_SUCCESS)
		return status;

	status = __ntapi->ldr_load_dll(
		0,
		0,
		&nt_image_name,
		image_base);

	return status;
}
