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

static int32_t __tt_create_process_cancel(nt_create_process_params * params, void * hsection, int32_t status)
{
	if (params->hprocess) {
		__ntapi->zw_terminate_process(params->hprocess,NT_STATUS_INTERNAL_ERROR);
		__ntapi->zw_close(params->hprocess);
	}

	if (params->hthread)
		__ntapi->zw_close(params->hthread);

	if (hsection)
		__ntapi->zw_close(hsection);

	return status;
}

int32_t __stdcall __ntapi_tt_create_native_process_v1(nt_create_process_params * params)
{
	int32_t				status;
	void *				hfile;
	void *				hsection;

	nt_object_attributes		oa_file;
	nt_object_attributes		oa_process;
	nt_object_attributes		oa_thread;

	nt_unicode_string		nt_image;
	nt_unicode_string		nt_cmd_line;
	nt_process_parameters *		rprocess_params;
	nt_thread_params		tparams;

	nt_io_status_block		iosb;
	nt_section_image_information	sii;

	wchar16_t *			cmd_line_runtime_buffer;
	size_t				cmd_line_runtime_buffer_size;
	int				fresume_thread;

	#if defined (__NT32)
	wchar16_t		runtime_arg[12] = {
		' ','-','r',' ',
		'i','n','t','e','g','r','a','l'};
	#elif defined (__NT64)
	wchar16_t		runtime_arg[20] = {
		' ','-','r',' ',
		'i','n','t','e','g','r','a','l',
		'-','r','u','n','t','i','m','e'};
	#endif

	/* validation */
	if (params->cmd_line && params->process_params)
		return NT_STATUS_INVALID_PARAMETER_MIX;
	else if (params->cmd_line && params->rtblock)
		return NT_STATUS_INVALID_PARAMETER_MIX;
	else if (params->environment && params->process_params)
		return NT_STATUS_INVALID_PARAMETER_MIX;

	/* tparams */
	__ntapi->tt_aligned_block_memset(
		&tparams, 0, sizeof(tparams));

	/* image_name */
	__ntapi->rtl_init_unicode_string(
		&nt_image,
		params->image_name);

	/* oa_process */
	if (!params->obj_attr_process) {
		__ntapi->tt_aligned_block_memset(
			&oa_process,0,sizeof(oa_process));

		oa_process.len = sizeof(oa_process);
		params->obj_attr_process = &oa_process;
	}

	/* oa_thread */
	if (!params->obj_attr_thread) {
		__ntapi->tt_aligned_block_memset(
			&oa_thread,0,sizeof(oa_thread));

		oa_thread.len = sizeof(oa_thread);
		params->obj_attr_thread = &oa_thread;
	}

	/* legacy tasks */
        /* init the oa_file structure */
	oa_file.len		= sizeof(nt_object_attributes);
	oa_file.root_dir	= (void *)0;
	oa_file.obj_name	= &nt_image;
	oa_file.obj_attr	= 0;
	oa_file.sec_desc	= (nt_security_descriptor *)0;
	oa_file.sec_qos		= (nt_sqos *)0;

	/* open the file */
	if ((status = __ntapi->zw_open_file(
			&hfile,
			NT_FILE_EXECUTE | NT_PROCESS_SYNCHRONIZE,
			&oa_file,
			&iosb,
			NT_FILE_SHARE_READ,
			NT_FILE_SYNCHRONOUS_IO_NONALERT)))
		return status;

	/* create the executable section */
	hsection	 = 0;
	oa_file.obj_name = 0;

	status = __ntapi->zw_create_section(
		&hsection,
		NT_SECTION_ALL_ACCESS,
		&oa_file,0,
		NT_PAGE_EXECUTE,
		NT_SEC_IMAGE,
		hfile);

	__ntapi->zw_close(hfile);
	if (status) return status;

	/* create the process */
	if ((status = __ntapi->zw_create_process(
			&params->hprocess,
			NT_PROCESS_ALL_ACCESS,
			&oa_process,
			NT_CURRENT_PROCESS_HANDLE,
			1,hsection,0,0)))
		return __tt_create_process_cancel(params,hsection,status);

	/* obtain stack/heap and entry point information */
	if ((status = __ntapi->zw_query_section(
			hsection,
			NT_SECTION_IMAGE_INFORMATION,
			&sii,sizeof(sii),0)))
		return __tt_create_process_cancel(params,hsection,status);

	/* obtain process information */
	if ((status = __ntapi->zw_query_information_process(
			tparams.hprocess,
			NT_PROCESS_BASIC_INFORMATION,
			&params->pbi,sizeof(params->pbi),
			0)))
		return __tt_create_process_cancel(params,hsection,status);

	/* create remote process parameters block */
	if (!params->process_params) {
		/* cmd_line */
		if (!params->cmd_line) {
			params->cmd_line = params->image_name;
		}

		__ntapi->rtl_init_unicode_string(
			&nt_cmd_line,
			params->cmd_line);

		/* rtblock */
		if (params->rtblock) {
			cmd_line_runtime_buffer = (wchar16_t *)0;
			cmd_line_runtime_buffer_size = nt_cmd_line.maxlen 
							+ sizeof(runtime_arg);

			if ((status = __ntapi->zw_allocate_virtual_memory(
					NT_CURRENT_PROCESS_HANDLE,
					(void **)&cmd_line_runtime_buffer,
					0,&cmd_line_runtime_buffer_size,
					NT_MEM_RESERVE | NT_MEM_COMMIT,
					NT_PAGE_READWRITE)))
				return __tt_create_process_cancel(params,hsection,status);

			__ntapi->tt_memcpy_utf16(
				(wchar16_t *)cmd_line_runtime_buffer,
				(wchar16_t *)nt_cmd_line.buffer,
				nt_cmd_line.strlen);

			__ntapi->tt_memcpy_utf16(
				(wchar16_t *)pe_va_from_rva(
					cmd_line_runtime_buffer,
					nt_cmd_line.strlen),
				(wchar16_t *)runtime_arg,
				sizeof(runtime_arg));

			nt_cmd_line.strlen += sizeof(runtime_arg);
			nt_cmd_line.maxlen += sizeof(runtime_arg);
			nt_cmd_line.buffer = cmd_line_runtime_buffer;
		}

		/* environment */
		if (!params->environment) {
			params->environment = __ntapi->tt_get_peb_env_block_utf16();
		}
	}

	fresume_thread = (params->creation_flags_thread ^ 0x01) & 0x01;

	/* create target thread */
	tparams.hprocess		= params->hprocess;
	tparams.start			= (nt_thread_start_routine *)sii.entry_point;
	tparams.obj_attr		= &oa_thread;
	tparams.creation_flags		= NT_CREATE_SUSPENDED | NT_CREATE_FIRST_THREAD_OF_PROCESS;
	tparams.stack_size_commit	= sii.stack_commit;
	tparams.stack_size_reserve	= sii.stack_reserve;

	if ((status = __ntapi->tt_create_remote_thread(&tparams)))
		return __tt_create_process_cancel(params,hsection,status);

	/* remote process params */
	if ((status = __ntapi->tt_create_remote_process_params(
			tparams.hprocess,
			&rprocess_params,
			&nt_image,
			(nt_unicode_string *)0,
			(nt_unicode_string *)0,
			&nt_cmd_line,
			params->environment,
			(nt_unicode_string *)0,
			(nt_unicode_string *)0,
			(nt_unicode_string *)0,
			(nt_unicode_string *)0)))
		return __tt_create_process_cancel(params,hsection,status);

	/* update the target process environment block: */
	/* make process_params point to rparams_block   */
	if ((status = __ntapi->zw_write_virtual_memory(
			tparams.hprocess,
			(char *)((uintptr_t)params->pbi.peb_base_address
				+ (uintptr_t)&(((nt_peb *)0)->process_params)),
			(char *)&rprocess_params,
			sizeof(uintptr_t),0)))
		return __tt_create_process_cancel(params,hsection,status);

	/* rtdata */
	if (params->rtblock && (status = __ntapi_tt_create_remote_runtime_data(tparams.hprocess,params->rtblock)))
		return __tt_create_process_cancel(params,hsection,status);

	if (fresume_thread && (status = __ntapi->zw_resume_thread(tparams.hthread,0)))
		return __tt_create_process_cancel(params,hsection,status);

	/* all done */
	params->hthread		= tparams.hthread;
	params->cid.process_id	= params->pbi.unique_process_id;
	params->cid.thread_id	= tparams.thread_id;

	return status;
}
