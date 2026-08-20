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

int32_t __stdcall __ntapi_tt_update_runtime_data(nt_runtime_data * rtdata)
{
	int32_t				status;
	nt_process_basic_information	pbi;
	uint32_t			ret;
	nt_oa				oa = {sizeof(oa)};

	/* process (self) */
	rtdata->cid_self.process_id = pe_get_current_process_id();
	rtdata->cid_self.thread_id  = 0;

	if ((status = __ntapi->zw_open_process(
			&rtdata->hprocess_self,
			NT_PROCESS_ALL_ACCESS,
			&oa,&rtdata->cid_self)))
		return status;

	if (rtdata->cid_parent.process_id)
		return NT_STATUS_SUCCESS;

	/* process (parent) */
	if ((status = __ntapi->zw_query_information_process(
			rtdata->hprocess_self,
			NT_PROCESS_BASIC_INFORMATION,
			&pbi,sizeof(pbi),&ret)))
		return status;

	rtdata->cid_parent.process_id = pbi.inherited_from_unique_process_id;
	rtdata->cid_parent.thread_id  = 0;
	rtdata->hprocess_parent = 0;

	return NT_STATUS_SUCCESS;
}

int32_t __stdcall __ntapi_tt_init_runtime_data(nt_runtime_data * rtdata)
{
	int32_t				status;
	nt_peb *			peb;
	nt_oa				oa = {sizeof(oa)};

	/* init */
	__ntapi->tt_aligned_block_memset(rtdata,0,sizeof(*rtdata));
	peb = (nt_peb *)(pe_get_peb_address());

	/* pid (self,parent) */
	if ((status = __ntapi_tt_update_runtime_data(rtdata)))
		return status;

	/* std handles */
	rtdata->hstdin  = peb->process_params->hstdin;
	rtdata->hstdout = peb->process_params->hstdout;
	rtdata->hstderr = peb->process_params->hstderr;

	if (__ntapi->tt_get_file_handle_type(rtdata->hstdin,&rtdata->stdin_type)) {
		rtdata->hstdin = NT_INVALID_HANDLE_VALUE;
		rtdata->stdin_type = 0;
	}

	if (__ntapi->tt_get_file_handle_type(rtdata->hstdout,&rtdata->stdout_type)) {
		rtdata->hstdout = NT_INVALID_HANDLE_VALUE;
		rtdata->stdout_type = 0;
	}

	if (__ntapi->tt_get_file_handle_type(rtdata->hstderr,&rtdata->stderr_type)) {
		rtdata->hstderr = NT_INVALID_HANDLE_VALUE;
		rtdata->stderr_type = 0;
	}

	return 0;
}
