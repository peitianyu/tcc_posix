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

/* __ntapi_dsr_connect_internal_client executes in the daemon's dedicated thread */
int32_t __stdcall __ntapi_dsr_connect_internal_client(nt_daemon_params * params)
{
	int32_t *		pstatus;

	intptr_t		port_id;
	nt_port_message		port_msg;
	nt_large_integer	timeout;
	void *			_hport_client;

	pstatus = &params->exit_code_daemon_start;

	/* timeout-enabled first connection */
	timeout.quad = NT_DSR_INIT_MAX_WAIT;

	*pstatus = __ntapi->zw_reply_wait_receive_port_ex(
		params->hport_daemon,
		&port_id,
		(nt_port_message *)0,
		(nt_port_message *)&port_msg,
		&timeout);

	if (*pstatus != NT_STATUS_SUCCESS)
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			*pstatus);

	/* the internal client must be first */
	if (port_msg.client_id.process_id != pe_get_current_process_id())
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			NT_STATUS_PORT_CONNECTION_REFUSED);

	/* accept connection request */
	*pstatus = __ntapi->zw_accept_connect_port(
		&_hport_client,
		port_msg.client_id.process_id,
		(nt_port_message *)&port_msg,
		NT_LPC_ACCEPT_CONNECTION,
		(nt_port_section_write *)0,
		(nt_port_section_read *)0);

	if (*pstatus != NT_STATUS_SUCCESS)
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			*pstatus);

	/* finalize connection */
	*pstatus = __ntapi->zw_complete_connect_port(_hport_client);

	if (*pstatus != NT_STATUS_SUCCESS)
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			*pstatus);

	return *pstatus;
}


/* __ntapi_dsr_internal_client_connect executes in its own temporary thread */
int32_t __stdcall __ntapi_dsr_internal_client_connect(nt_daemon_params * params)
{
	int32_t *			pstatus;

	nt_unicode_string		server_name;
	nt_object_attributes		oa;
	nt_security_quality_of_service	sqos;
	nt_large_integer		timeout;

	pstatus = &params->exit_code_internal_client;

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

	/* wait for the server to be ready */
	timeout.quad = NT_DSR_INIT_MAX_WAIT;

	if ((*pstatus = __ntapi->zw_wait_for_single_object(
			params->hevent_daemon_ready,
			0,&timeout)))
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			*pstatus);

	/* establish internal connection */
	*pstatus = __ntapi->zw_connect_port(
		&params->hport_internal_client,
		&server_name,
		&sqos,
		0,0,0,0,0);

	if (*pstatus != NT_STATUS_SUCCESS)
		__ntapi->zw_terminate_thread(
			NT_CURRENT_THREAD_HANDLE,
			*pstatus);

	/* return port info */
	if (params->pport_internal_client)
		*(params->pport_internal_client) = params->hport_internal_client;

	/* signal the 'internal-client-is-ready' event */
	*pstatus = __ntapi->zw_set_event(
		params->hevent_internal_client_ready,
		0);

	/* exit the task-specific thread */
	__ntapi->zw_terminate_thread(
		NT_CURRENT_THREAD_HANDLE,
		*pstatus);

	/* (no return) */
	return NT_STATUS_INTERNAL_ERROR;
}
