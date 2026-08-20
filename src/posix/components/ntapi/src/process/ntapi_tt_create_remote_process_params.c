/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_thread.h>
#include <ntapi/nt_process.h>
#include <ntapi/nt_string.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t	__stdcall __ntapi_tt_create_remote_process_params(
	__in	void *				hprocess,
	__out 	nt_process_parameters **	rprocess_params,
	__in	nt_unicode_string *		image_file,
	__in	nt_unicode_string *		dll_path		__optional,
	__in	nt_unicode_string *		current_directory	__optional,
	__in	nt_unicode_string *		command_line		__optional,
	__in	wchar16_t *			environment 		__optional,
	__in	nt_unicode_string *		window_title		__optional,
	__in	nt_unicode_string *		desktop_info		__optional,
	__in	nt_unicode_string *		shell_info		__optional,
	__in	nt_unicode_string *		runtime_data		__optional)
{
	#define __ALIGN_ALLOC_SIZE \
		process_params.alloc_size += sizeof(uintptr_t) - 1; \
		process_params.alloc_size /= sizeof(uintptr_t); \
		process_params.alloc_size *= sizeof(uintptr_t);

	int32_t		status;

	ptrdiff_t	d_image;
	ptrdiff_t	d_dll_path;
	ptrdiff_t	d_cwd;
	ptrdiff_t	d_cmd_line;
	ptrdiff_t	d_environment;
	ptrdiff_t	d_runtime;
	/*
	ptrdiff_t	d_wnd_title;
	ptrdiff_t	d_desktop;
	ptrdiff_t	d_shell;
	*/

	wchar16_t *	wch;
	size_t		env_block_size;
	size_t		params_block_size;
	size_t		bytes_written;

	nt_process_parameters	process_params;
	nt_process_parameters *	params_block;
	nt_process_parameters *	rparams_block;
	nt_process_parameters *	params_default;

	/* make the compiler happy */
	d_image		= 0;
	d_dll_path	= 0;
	d_cwd		= 0;
	d_cmd_line	= 0;
	d_environment	= 0;
	d_runtime	= 0;
	env_block_size	= 0;

	/* initialize */
	__ntapi->tt_aligned_block_memset(
		&process_params,
		0,sizeof(nt_process_parameters));

	/* allow for extended structures (newer OS versions) */
	process_params.alloc_size = sizeof(nt_process_parameters)
					+ 8 * sizeof(uintptr_t);

	params_default = ((nt_peb *)pe_get_peb_address())->process_params;

	/* image_file */
	if (image_file) {
		/* check alignment and sanity */
		if ((uintptr_t)image_file->buffer % sizeof(uintptr_t))
			return NT_STATUS_INVALID_PARAMETER_2;
		else if (image_file->maxlen < image_file->strlen)
			return NT_STATUS_INVALID_PARAMETER_2;

		process_params.image_file_name.strlen = image_file->strlen;
		process_params.image_file_name.maxlen = image_file->maxlen;

		/* store offset and update alloc_size */
		d_image = process_params.alloc_size;
		process_params.alloc_size += image_file->maxlen;
		__ALIGN_ALLOC_SIZE;
	}

	/* dll_path */
	if (!dll_path)
		dll_path = &(params_default->dll_path);

	if (dll_path) {
		/* check alignment and sanity */
		if ((uintptr_t)dll_path->buffer % sizeof(uintptr_t))
			return NT_STATUS_INVALID_PARAMETER_3;
		else if (dll_path->maxlen < dll_path->strlen)
			return NT_STATUS_INVALID_PARAMETER_3;

		process_params.dll_path.strlen = dll_path->strlen;
		process_params.dll_path.maxlen = dll_path->maxlen;

		/* store offset and update alloc_size */
		d_dll_path = process_params.alloc_size;
		process_params.alloc_size += dll_path->maxlen;
		__ALIGN_ALLOC_SIZE;
	}

	/* current_directory */
	if (!current_directory)
		current_directory = &(params_default->cwd_name);

	if (current_directory) {
		/* check alignment and sanity */
		if ((uintptr_t)current_directory->buffer % sizeof(uintptr_t))
			return NT_STATUS_INVALID_PARAMETER_4;
		else if (current_directory->maxlen < current_directory->strlen)
			return NT_STATUS_INVALID_PARAMETER_4;

		process_params.cwd_name.strlen = current_directory->strlen;
		process_params.cwd_name.maxlen = current_directory->maxlen;

		/* store offset and update alloc_size */
		d_cwd = process_params.alloc_size;
		process_params.alloc_size += current_directory->maxlen;
		__ALIGN_ALLOC_SIZE;
	}

	/* command_line */
	if (command_line) {
		/* check alignment and sanity */
		if ((uintptr_t)command_line->buffer % sizeof(uintptr_t))
			return NT_STATUS_INVALID_PARAMETER_5;
		else if (command_line->maxlen < command_line->strlen)
			return NT_STATUS_INVALID_PARAMETER_5;

		process_params.command_line.strlen = command_line->strlen;
		process_params.command_line.maxlen = command_line->maxlen;

		/* store offset and update alloc_size */
		d_cmd_line = process_params.alloc_size;
		process_params.alloc_size += command_line->maxlen;
		__ALIGN_ALLOC_SIZE;
	}

	/* environment */
	if (environment) {
		/* check alignment */
		if ((uintptr_t)environment % sizeof(uintptr_t))
			return NT_STATUS_INVALID_PARAMETER_6;

		/* obtain size of environment block */
		wch = environment;

		while (*wch) {
			/* reach the end of the current variable */
			while (*wch++)
			/* proceed to the next variable */
			wch++;
		}

		env_block_size = (uintptr_t)wch - (uintptr_t)environment;

		/* store offset and update alloc_size */
		d_environment = process_params.alloc_size;
		process_params.alloc_size += (uint32_t)env_block_size + 0x1000;
		__ALIGN_ALLOC_SIZE;
	}

	/* runtime_data */
	if (runtime_data) {
		/* check alignment and sanity */
		if ((uintptr_t)runtime_data->buffer % sizeof(uintptr_t))
			return NT_STATUS_INVALID_PARAMETER_5;
		else if (runtime_data->maxlen < runtime_data->strlen)
			return NT_STATUS_INVALID_PARAMETER_5;

		process_params.runtime_data.strlen = runtime_data->strlen;
		process_params.runtime_data.maxlen = runtime_data->maxlen;

		/* store offset and update alloc_size */
		d_runtime = process_params.alloc_size;
		process_params.alloc_size += runtime_data->maxlen;
		__ALIGN_ALLOC_SIZE;
	}

	/* allocate local and remote process parameters blocks */
	params_block = (nt_process_parameters *)0;
	rparams_block = (nt_process_parameters *)0;

	process_params.used_size = process_params.alloc_size;
	params_block_size = process_params.alloc_size;

	/* local block */
	status = __ntapi->zw_allocate_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		(void **)&params_block,
		0,
		&params_block_size,
		NT_MEM_COMMIT,
		NT_PAGE_READWRITE);

	if (status != NT_STATUS_SUCCESS)
		return status;

	process_params.alloc_size = (uint32_t)params_block_size;
	__ntapi->tt_aligned_block_memset(params_block,0,params_block_size);

	/* remote block */
	status = __ntapi->zw_allocate_virtual_memory(
		hprocess,
		(void **)&rparams_block,
		0,
		&params_block_size,
		NT_MEM_RESERVE | NT_MEM_COMMIT,
		NT_PAGE_READWRITE);

	if (status != NT_STATUS_SUCCESS) {
		__ntapi->zw_free_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&params_block,
			(size_t *)&process_params.alloc_size,
			NT_MEM_RELEASE);

		return status;
	}

	/* copy the process_params structure */
	__ntapi->tt_aligned_memcpy_utf16(
		(uintptr_t *)params_block,
		(uintptr_t *)&process_params,
		sizeof(nt_process_parameters));

	/* image_file */
	if (image_file) {
		params_block->image_file_name.buffer =
			(uint16_t *)pe_va_from_rva(rparams_block,d_image);

		__ntapi->tt_aligned_memcpy_utf16(
			(uintptr_t *)pe_va_from_rva(params_block,d_image),
			(uintptr_t *)image_file->buffer,
			image_file->strlen);
	}

	/* dll_path */
	if (dll_path) {
		params_block->dll_path.buffer =
			(uint16_t *)pe_va_from_rva(rparams_block,d_dll_path);

		__ntapi->tt_aligned_memcpy_utf16(
			(uintptr_t *)pe_va_from_rva(params_block,d_dll_path),
			(uintptr_t *)dll_path->buffer,
			dll_path->strlen);
	}

	/* current_directory */
	if (current_directory) {
		params_block->cwd_name.buffer =
			(uint16_t *)pe_va_from_rva(rparams_block,d_cwd);

		__ntapi->tt_aligned_memcpy_utf16(
			(uintptr_t *)pe_va_from_rva(params_block,d_cwd),
			(uintptr_t *)current_directory->buffer,
			current_directory->strlen);
	}

	/* command_line */
	if (command_line) {
		params_block->command_line.buffer =
			(uint16_t *)pe_va_from_rva(rparams_block,d_cmd_line);

		__ntapi->tt_aligned_memcpy_utf16(
			(uintptr_t *)pe_va_from_rva(params_block,d_cmd_line),
			(uintptr_t *)command_line->buffer,
			command_line->strlen);
	}

	/* environment */
	if (environment) {
		params_block->environment =
			(wchar16_t *)pe_va_from_rva(rparams_block,d_environment);

		__ntapi->tt_aligned_memcpy_utf16(
			(uintptr_t *)pe_va_from_rva(params_block,d_environment),
			(uintptr_t *)environment,
			env_block_size);
	}

	/* runtime_data */
	if (runtime_data) {
		params_block->runtime_data.buffer =
			(uint16_t *)pe_va_from_rva(rparams_block,d_runtime);

		__ntapi->tt_aligned_memcpy_utf16(
			(uintptr_t *)pe_va_from_rva(params_block,d_runtime),
			(uintptr_t *)runtime_data->buffer,
			runtime_data->strlen);
	}

	params_block->flags = 1; /* normalized */

	/* copy the local params block to the remote process */
	status = __ntapi->zw_write_virtual_memory(
		hprocess,
		rparams_block,
		(char *)params_block,
		process_params.alloc_size,
		&bytes_written);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* free the local params block */
	__ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		(void **)&params_block,
		(size_t *)&process_params.alloc_size,
		NT_MEM_RELEASE);

	/* all done */
	*rprocess_params = rparams_block;

	return NT_STATUS_SUCCESS;
}
