/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_file.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_open_logical_parent_directory(
	__out	void **		hparent,
	__in	void *		hdir,
	__out	uintptr_t *	buffer,
	__in	uint32_t	buffer_size,
	__in	uint32_t	desired_access,
	__in	uint32_t	open_options,
	__out	int32_t *	type)
{
	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}
