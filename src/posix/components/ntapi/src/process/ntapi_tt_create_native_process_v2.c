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

static int32_t __tt_create_process_cancel(nt_create_process_params * params, int32_t status)
{
	if (params->hprocess) {
		__ntapi->zw_terminate_process(params->hprocess,NT_STATUS_INTERNAL_ERROR);
		__ntapi->zw_close(params->hprocess);
	}

	if (params->hthread)
		__ntapi->zw_close(params->hthread);

	return status;
}


int32_t __stdcall __ntapi_tt_create_native_process_v2(
	__in_out	nt_create_process_params *	params)
{
	int32_t			status;

	nt_object_attributes	oa_process;
	nt_object_attributes	oa_thread;

	nt_unicode_string	nt_image;
	nt_unicode_string	nt_cmd_line;
	wchar16_t *		cmd_line_runtime_buffer;
	size_t			cmd_line_runtime_buffer_size;

	nt_create_process_info	nt_process_info;
	int			fresume_thread;

	struct {
		size_t				size_in_bytes;
		nt_create_process_ext_param	file_info;
	} ext_params;

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

	/* process_params */
	if (!params->process_params) {
		/* cmd_line */
		if (!params->cmd_line) {
			params->cmd_line = params->image_name;
		}

		__ntapi->rtl_init_unicode_string(
			&nt_cmd_line,
			params->cmd_line);

		/* rtdata (alternative to cmd_line) */
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
				return status;

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
		if (!params->environment)
			params->environment = __ntapi->tt_get_peb_env_block_utf16();

		if ((status = __ntapi->rtl_create_process_parameters(
				&params->process_params,
				&nt_image,
				(nt_unicode_string *)0,
				(nt_unicode_string *)0,
				&nt_cmd_line,
				params->environment,
				(nt_unicode_string *)0,
				(nt_unicode_string *)0,
				(nt_unicode_string *)0,
				(nt_unicode_string *)0)))
			return status;

		__ntapi->rtl_normalize_process_params(params->process_params);
	}

	/* create_process_info */
	if (!params->create_process_info) {
		__ntapi->tt_aligned_block_memset(
			&nt_process_info,0,sizeof(nt_process_info));

		nt_process_info.size			   = sizeof(nt_create_process_info);
		nt_process_info.state			   = NT_PROCESS_CREATE_INITIAL_STATE;
		nt_process_info.init_state.init_flags	   = NT_PROCESS_CREATE_INFO_OBTAIN_OUTPUT;
		nt_process_info.init_state.file_access_ext = NT_FILE_READ_ATTRIBUTES|NT_FILE_READ_ACCESS;

		params->create_process_info = &nt_process_info;
	}

	/* create_process_ext_params */
	if (!params->create_process_ext_params) {
		__ntapi->tt_aligned_block_memset(
			&ext_params,0,sizeof(ext_params));

		ext_params.size_in_bytes = sizeof(ext_params);

		/* file_info */
		ext_params.file_info.ext_param_type	= NT_CREATE_PROCESS_EXT_PARAM_SET_FILE_NAME;
		ext_params.file_info.ext_param_size	= nt_image.strlen;
		ext_params.file_info.ext_param_addr	= nt_image.buffer;

		params->create_process_ext_params = (nt_create_process_ext_params *)&ext_params;
	}

	params->hprocess = 0;
	params->hthread  = 0;
	fresume_thread   = 0;

	if (params->rtblock) {
		fresume_thread = (params->creation_flags_thread ^ 0x01) & 0x01;
		params->creation_flags_thread |= 0x01;
	}

	if (!params->desired_access_process)
		params->desired_access_process = NT_PROCESS_ALL_ACCESS;

	if (!params->desired_access_thread)
		params->desired_access_thread = NT_THREAD_ALL_ACCESS;

	if ((status = __ntapi->zw_create_user_process(
			&params->hprocess,
			&params->hthread,
			params->desired_access_process,
			params->desired_access_thread,
			params->obj_attr_process,
			params->obj_attr_thread,
			params->creation_flags_process,
			params->creation_flags_thread,
			params->process_params,
			params->create_process_info,
			params->create_process_ext_params)))
		return status;

	if ((status = __ntapi->zw_query_information_process(
			params->hprocess,
			NT_PROCESS_BASIC_INFORMATION,
			&params->pbi,sizeof(params->pbi),
			0)))
		return __tt_create_process_cancel(params,status);

	if (!params->rtblock)
		return NT_STATUS_SUCCESS;

	/* rtdata */
	if ((status = __ntapi_tt_create_remote_runtime_data(params->hprocess,params->rtblock)))
		return __tt_create_process_cancel(params,status);

	/* conditional resume */
	if (fresume_thread && (status = __ntapi->zw_resume_thread(params->hthread,0)))
		return __tt_create_process_cancel(params,status);

	return NT_STATUS_SUCCESS;
}
