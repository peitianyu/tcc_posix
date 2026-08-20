/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/nt_status.h>
#include <ntapi/nt_thread.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_daemon.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


static void __stdcall __ntapi_dsr_once(nt_daemon_params * params);

int32_t __stdcall __ntapi_dsr_init(nt_daemon_params * params)
{
	int32_t			status;

	nt_thread_params	tparams;
	nt_large_integer	timeout;

	/* port_keys */
	if (params->flags & NT_DSR_INIT_GENERATE_KEYS)
		if ((status = __ntapi->tt_port_generate_keys(params->port_keys)))
			return status;

	/* port_name_keys */
	if (params->flags & NT_DSR_INIT_FORMAT_KEYS)
		__ntapi->tt_port_format_keys(
			params->port_keys,
			params->port_name_keys);

	/* 'daemon-is-ready' event */
	if (!params->hevent_daemon_ready) {
		if ((status = __ntapi->tt_create_private_event(
				&params->hevent_daemon_ready,
				NT_NOTIFICATION_EVENT,
				NT_EVENT_NOT_SIGNALED)))
			return status;

		if (params->pevent_daemon_ready)
			*(params->pevent_daemon_ready) = params->hevent_daemon_ready;
	}

	/* 'internal-client-is-ready' event */
	if (!params->hevent_internal_client_ready) {
		if ((status = __ntapi->tt_create_inheritable_event(
				&params->hevent_internal_client_ready,
				NT_NOTIFICATION_EVENT,
				NT_EVENT_NOT_SIGNALED)))
			return status;

		if (params->pevent_internal_client_ready)
			*(params->pevent_internal_client_ready) = params->hevent_internal_client_ready;
	}

	/* daemon dedicated thread: general parameters */
	__ntapi->tt_aligned_block_memset(
		&tparams,0,sizeof(tparams));

	tparams.start = (nt_thread_start_routine *)__ntapi_dsr_start;
	tparams.arg   = params;

	/* daemon dedicated thread: stack parameters (optional) */
	tparams.stack_size_commit	= params->stack_size_commit;
	tparams.stack_size_reserve	= params->stack_size_reserve;
	tparams.stack_info		= params->stack_info;

	/* daemon dedicated thread: create */
	status = __ntapi->tt_create_local_thread(&tparams);
	params->hthread_daemon_loop = tparams.hthread;
	if (status) return status;

	/* daemon dedicated thread: actual stack size */
	params->stack_size_commit	= tparams.stack_size_commit;
	params->stack_size_reserve	= tparams.stack_size_reserve;


	/* establish internal connection */
	__ntapi->tt_aligned_block_memset(
		&tparams,0,sizeof(tparams));

	tparams.start = (nt_thread_start_routine *)__ntapi_dsr_internal_client_connect;
	tparams.arg   = params;

	status = __ntapi->tt_create_local_thread(&tparams);
	params->hthread_internal_client = tparams.hthread;
	if (status) return status;

	/* wait until the internal connection had been established */
	timeout.quad = NT_DSR_INIT_MAX_WAIT;

	status = __ntapi->zw_wait_for_single_object(
		params->hevent_internal_client_ready,
		0,
		&timeout);


	if (params->flags & NT_DSR_INIT_CLOSE_EVENTS) {
		__ntapi->zw_close(params->hevent_daemon_ready);
		__ntapi->zw_close(params->hevent_internal_client_ready);
	}

	return status;
}


/* __ntapi_dsr_start executes in the daemon's dedicated thread */
int32_t __stdcall __ntapi_dsr_start(nt_daemon_params * params)
{
	__ntapi_dsr_once(params);
	__ntapi_dsr_create_port(params);
	__ntapi_dsr_connect_internal_client(params);
	params->daemon_loop_routine(params->daemon_loop_context);

	/* (no return) */
	return NT_STATUS_INTERNAL_ERROR;
}

/* __ntapi_dsr_once executes in the daemon's dedicated thread */
static void __stdcall __ntapi_dsr_once(nt_daemon_params * params)
{
	int32_t status;

	if (!params->daemon_once_routine)
		return;

	if ((status = params->daemon_once_routine(params->daemon_loop_context))) {
		params->exit_code_daemon_start = status;
		__ntapi->zw_terminate_thread(NT_CURRENT_THREAD_HANDLE,status);
	}
}

/* __ntapi_dsr_create_port executes in the daemon's dedicated thread */
int32_t __stdcall __ntapi_dsr_create_port(nt_daemon_params * params)
{
	int32_t *			pstatus;
	nt_object_attributes		oa;
	nt_security_quality_of_service	sqos;
	nt_unicode_string		server_name;

	pstatus = &params->exit_code_daemon_start;

	/* init server_name */
	server_name.strlen = (uint16_t)__ntapi->tt_string_null_offset_short((const int16_t *)params->port_name);
	server_name.maxlen = 0;
	server_name.buffer = (uint16_t *)params->port_name;

	/* init security structure */
	sqos.length 			= sizeof(sqos);
	sqos.impersonation_level	= NT_SECURITY_IMPERSONATION;
	sqos.context_tracking_mode	= NT_SECURITY_TRACKING_DYNAMIC;
	sqos.effective_only		= 1;

	/* init the port's object attributes */
	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &server_name;
	oa.obj_attr	= 0;
	oa.sec_desc	= (nt_security_descriptor *)0;
	oa.sec_qos	= &sqos;

	/* create the port */
	*pstatus = __ntapi->zw_create_port(
		&params->hport_daemon,
		&oa,0,(uint32_t)params->port_msg_size,
		0);

	if (*pstatus != NT_STATUS_SUCCESS)
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			*pstatus);

	/* return port info */
	if (params->pport_daemon)
		*(params->pport_daemon) = params->hport_daemon;

	/* signal the daemon-is-ready event */
	*pstatus = __ntapi->zw_set_event(
		params->hevent_daemon_ready,
		(int32_t *)0);

	if (*pstatus != NT_STATUS_SUCCESS)
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			*pstatus);

	return *pstatus;
}
