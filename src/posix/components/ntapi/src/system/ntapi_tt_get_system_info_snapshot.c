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
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_get_system_info_snapshot(
	__in_out nt_system_information_snapshot * sys_info_snapshot)
{
	int32_t		status;

	/* pre-allocated buffer? */
	if (sys_info_snapshot->buffer)
		status = __ntapi->zw_query_system_information(
			sys_info_snapshot->sys_info_class,
			sys_info_snapshot->buffer,
			sys_info_snapshot->max_len,
			&sys_info_snapshot->info_len);
	else {
		/* set initial buffer size */
		sys_info_snapshot->max_len = NT_ALLOCATION_GRANULARITY;

		/* allocate initial buffer */
		status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&sys_info_snapshot->buffer,
			0,
			&sys_info_snapshot->max_len,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE);

		/* verification */
		if (status != NT_STATUS_SUCCESS)
			return status;

		/* loop until buffer is large enough to satisfy the system */
		while ((status = __ntapi->zw_query_system_information(
					sys_info_snapshot->sys_info_class,
					sys_info_snapshot->buffer,
					sys_info_snapshot->max_len,
					&sys_info_snapshot->info_len))
				== NT_STATUS_INFO_LENGTH_MISMATCH) {

			/* free previously allocated memory */
			status = __ntapi->zw_free_virtual_memory(
					NT_CURRENT_PROCESS_HANDLE,
					(void **)&sys_info_snapshot->buffer,
					&sys_info_snapshot->max_len,
					NT_MEM_RELEASE);

			/* verification */
			if (status != NT_STATUS_SUCCESS)
				return status;

			/* reset buffer and increase buffer size */
			sys_info_snapshot->buffer = (nt_system_information_buffer *)0;
			sys_info_snapshot->max_len += NT_ALLOCATION_GRANULARITY;

			/* reallocate buffer memory */
			status = __ntapi->zw_allocate_virtual_memory(
					NT_CURRENT_PROCESS_HANDLE,
					(void **)&sys_info_snapshot->buffer,
					0,
					&sys_info_snapshot->max_len,
					NT_MEM_COMMIT,
					NT_PAGE_READWRITE);

			/* verification */
			if (status != NT_STATUS_SUCCESS)
				return status;
		}
	}

	/* verification */
	if (status == NT_STATUS_SUCCESS) {
		sys_info_snapshot->pcurrent = &sys_info_snapshot->buffer->mark;
		return NT_STATUS_SUCCESS;
	} else {
		sys_info_snapshot->pcurrent = (void *)0;
		return status;
	}
}
