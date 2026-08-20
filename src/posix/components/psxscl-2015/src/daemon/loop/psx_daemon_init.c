/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_impl.h"
#include "psx_daemon.h"
#include "psx.h"

int32_t __stdcall __psx_daemon_once(void * context)
{
	int32_t priority = 15;

	rtdata->cid_self.thread_id = pe_get_current_thread_id();

	__ntapi->tt_sync_block_init(
		&ofd_lock,0,
		(int32_t)rtdata->cid_self.thread_id,
		0,0,0);

	__ntapi->tt_sync_block_init(
		&heap_lock,0,
		(int32_t)rtdata->cid_self.thread_id,
		0,0,0);

	__ntapi->tt_sync_block_init(
		&mman_lock,0,
		(int32_t)rtdata->cid_self.thread_id,
		0,0,0);

	__ntapi->tt_sync_block_init(
		&pid_lock,0,
		(int32_t)rtdata->cid_self.thread_id,
		0,0,0);

	__ntapi->tt_sync_block_init(
		&sigfn_lock,0,
		(int32_t)rtdata->cid_self.thread_id,
		0,0,0);

	return __ntapi->zw_set_information_thread(
		NT_CURRENT_THREAD_HANDLE,
		NT_THREAD_PRIORITY,
		&priority,sizeof(priority));
}


int32_t __stdcall __psx_daemon_init(void * context)
{
	int32_t				status;
	nt_daemon_params		dparams;

	/* daemon attributes */
	daemon_attr.type	= NT_PORT_TYPE_DAEMON;
	daemon_attr.subtype	= NT_PORT_SUBTYPE_DEFAULT;

	/* port guid */
	status = __ntapi->tt_port_guid_from_type(
		&daemon_attr.guid,
		daemon_attr.type,
		daemon_attr.subtype);

	if (status) return status;

	/* port keys */
	if ((status = __ntapi->tt_port_generate_keys(&daemon_attr.keys)))
		return status;

	/* port name */
	__ntapi->tt_port_name_from_attributes(
		&daemon_name,
		&daemon_attr);

	/* dparams */
	__ntapi->tt_aligned_block_memset(
		&dparams, 0, sizeof(dparams));

	dparams.port_name	= (wchar16_t *)&daemon_name;
	dparams.port_keys	= &daemon_keys;
	dparams.port_name_keys	= (nt_port_name_keys *)&daemon_name.port_name_keys;

	dparams.port_msg_size	= sizeof(struct __port_msg);
	dparams.flags		= NT_DSR_INIT_DEFAULT;

	dparams.daemon_once_routine	= __psx_daemon_once;
	dparams.daemon_loop_routine	= __psx_daemon_loop;
	dparams.daemon_loop_context	= &daemon_name;

	dparams.pport_daemon		= &hport_daemon;
	dparams.pport_internal_client	= &hport_internal_client;

	dparams.pevent_daemon_ready		= &__psx.__hevent_daemon_ready;
	dparams.pevent_internal_client_ready	= &__psx.__hevent_internal_client_ready;

	dparams.stack_size_commit		= __PSX_PAGE_SIZE;
	dparams.stack_size_reserve		= __PSX_PAGE_SIZE;

	return __ntapi->dsr_init(&dparams);
}
