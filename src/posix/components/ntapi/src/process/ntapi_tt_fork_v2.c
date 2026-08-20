/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_memory.h>
#include <ntapi/nt_thread.h>
#include <ntapi/nt_process.h>
#include <ntapi/nt_sync.h>
#include <ntapi/nt_string.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

static intptr_t __tt_fork_cancel(void * hprocess,int32_t status)
{
	__ntapi->zw_terminate_process(hprocess, status);
	__ntapi->zw_close(hprocess);
	return (intptr_t)(-1);
}

intptr_t __fastcall __tt_fork_impl_v2(
	__out	void **	hprocess,
	__out	void **	hthread)
{
	int32_t			status;
	void **			hport_session;
	nt_object_attributes	oa_process;
	nt_object_attributes	oa_thread;
	nt_create_process_info	process_info;
	nt_cid			cid;
	nt_sec_img_inf		sec_img_inf;
	nt_timeout		timeout;
	ntapi_internals *	__internals;

	struct {
		size_t				size_in_bytes;
		nt_create_process_ext_param	process_info;
		nt_create_process_ext_param	section_info;
	} ext_params;


	oa_process.len		= sizeof(nt_object_attributes);
	oa_process.root_dir	= 0;
	oa_process.obj_name	= 0;
	oa_process.obj_attr	= 0;
	oa_process.sec_desc	= 0;
	oa_process.sec_qos	= 0;

	oa_thread.len		= sizeof(nt_object_attributes);
	oa_thread.root_dir	= 0;
	oa_thread.obj_name	= 0;
	oa_thread.obj_attr	= 0;
	oa_thread.sec_desc	= 0;
	oa_thread.sec_qos	= 0;


	__ntapi->tt_aligned_block_memset(
		&process_info,0,sizeof(process_info));

	process_info.size			= sizeof(process_info);
	process_info.state			= NT_PROCESS_CREATE_INITIAL_STATE;
	process_info.init_state.init_flags	= NT_PROCESS_CREATE_FLAGS_NO_OBJECT_SYNC;

	__ntapi->tt_aligned_block_memset(&ext_params,0,sizeof(ext_params));
	__ntapi->tt_aligned_block_memset(&cid,0,sizeof(cid));
	__ntapi->tt_aligned_block_memset(&sec_img_inf,0,sizeof(sec_img_inf));
	ext_params.size_in_bytes = sizeof(ext_params);

	ext_params.process_info.ext_param_type	= NT_CREATE_PROCESS_EXT_PARAM_GET_CLIENT_ID;
	ext_params.process_info.ext_param_size	= sizeof(cid);
	ext_params.process_info.ext_param_addr	= &cid;

	ext_params.section_info.ext_param_type	= NT_CREATE_PROCESS_EXT_PARAM_GET_SECTION_IMAGE_INFO;
	ext_params.section_info.ext_param_size	= sizeof(sec_img_inf);
	ext_params.section_info.ext_param_addr	= &sec_img_inf;


	/* [thou shalt remember the single step paradox] */
	status = __ntapi->zw_create_user_process(
		hprocess,
		hthread,
		NT_PROCESS_ALL_ACCESS,
		NT_THREAD_ALL_ACCESS,
		&oa_process,
		&oa_thread,
		NT_PROCESS_CREATE_FLAGS_INHERIT_HANDLES,
		NT_PROCESS_CREATE_FLAGS_CREATE_THREAD_SUSPENDED,
		(nt_process_parameters *)0,
		&process_info,
		(nt_create_process_ext_params *)&ext_params);

	if (status == NT_STATUS_PROCESS_CLONED)
		return 0;
	else if (status)
		return (intptr_t)(-1);

	__internals	= __ntapi_internals();
	hport_session	= &__internals->hport_tty_session;
	timeout.quad	= (-1) * 10 * 1000 * __NT_FORK_CHILD_WAIT_MILLISEC;

	if (hport_session && *hport_session)
		if ((status = __ntapi->tty_client_process_register(
				*hport_session,
				cid.process_id,
				0,0,&timeout)))
			return __tt_fork_cancel(*hprocess,status);

	/* [thou shalt remember the single step paradox] */
	if ((status = __ntapi->zw_resume_thread(
			*hthread,0)))
		return __tt_fork_cancel(*hprocess,status);

	/* hoppla */
	return (int32_t)cid.process_id;
}

intptr_t __fastcall __ntapi_tt_fork_v2(
	__out	void **		hprocess,
	__out	void **		hthread)
{
	int32_t			status;
	intptr_t		pid;
	nt_large_integer	timeout;
	void **			hport_session;
	void *			hevent_tty_connected;
	ntapi_internals *	__internals;

	__internals	= __ntapi_internals();
	hport_session	= &__internals->hport_tty_session;
	timeout.quad	= (-1) * 10 * 1000 * __NT_FORK_CHILD_WAIT_MILLISEC;

	if (hport_session && *hport_session)
		if (__ntapi_tt_create_inheritable_event(
				&hevent_tty_connected,
				NT_NOTIFICATION_EVENT,
				NT_EVENT_NOT_SIGNALED))
			return (intptr_t)(-1);

	pid = __tt_fork_impl_v2(hprocess,hthread);

	if (!hport_session || !*hport_session)
		return pid;
	else if (pid < 0)
		return pid;

	if (pid == 0) {
		if ((status = __ntapi->tty_connect(
				hport_session,
				__internals->subsystem->base_named_objects,
				NT_SECURITY_IMPERSONATION)))
			return __tt_fork_cancel(NT_CURRENT_PROCESS_HANDLE,status);

			__internals->hdev_mount_point_mgr = 0;

			if (__internals->rtdata)
				__internals->rtdata->hsession = *hport_session;

			__ntapi->zw_set_event(
				hevent_tty_connected,
				0);
	} else {
		status = __ntapi->zw_wait_for_single_object(
			hevent_tty_connected,
			NT_SYNC_NON_ALERTABLE,
			&timeout);

		if (status && __PSX_DEBUG)
			if ((status = __ntapi->zw_wait_for_single_object(
					hevent_tty_connected,
					NT_SYNC_NON_ALERTABLE,
					0)))
				pid = __tt_fork_cancel(*hprocess,status);
	}


	__ntapi->zw_close(hevent_tty_connected);

	return pid;
}
