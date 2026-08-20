/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_memory.h>
#include <ntapi/nt_section.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_get_section_name(
	__in	void *			addr,
	__out	nt_mem_sec_name *	buffer,
	__in	uint32_t		buffer_size)
{
	size_t len;

	/* init buffer */
	buffer->section_name.strlen = 0;
	buffer->section_name.maxlen = (uint16_t)(buffer_size - sizeof(nt_unicode_string));
	buffer->section_name.buffer = buffer->section_name_buffer;

	return __ntapi->zw_query_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		addr,
		NT_MEMORY_SECTION_NAME,
		buffer,
		buffer_size,
		&len);
}
