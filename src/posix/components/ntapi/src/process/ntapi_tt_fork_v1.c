/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_atomic.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_memory.h>
#include <ntapi/nt_thread.h>
#include <ntapi/nt_process.h>
#include <ntapi/nt_string.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

intptr_t __cdecl	__attr_hidden__ __tt_fork_v1(void);
uint32_t __fastcall	__attr_hidden__ __tt_fork_child_entry_point(uintptr_t saved_regs_stack_pointer);
uint32_t __fastcall	__attr_hidden__ __tt_fork_child_entry_point_adj(uintptr_t saved_regs_stack_pointer);

/** legacy fork chronology:
 *
 *  parent:
 *	__ntapi_tt_fork ->
 *		__tt_fork ->
 *			__tt_fork_impl ->
 *		return to __tt_fork -->
 *	__ntapi_tt_fork
 *	-> return to caller
 *
 *  child:
 *		__tt_fork_child_entry_point[_adj] ->
 *	__ntapi_tt_fork (internal return) ->
 *	-> return to caller
**/


static intptr_t __tt_fork_cancel(void * hprocess,int32_t status)
{
	__ntapi->zw_terminate_process(hprocess, status);
	__ntapi->zw_close(hprocess);
	return (intptr_t)(-1);
}

intptr_t __fastcall __tt_fork_impl_v1(
	uintptr_t	saved_regs_stack_pointer,
	uintptr_t	stack_adjustment)
{
	int32_t			status;
	void *			hprocess;
	void *			hthread;
	void **			hport_session;
	ntapi_internals *	__internals;

	nt_object_attributes		oa;
	nt_process_basic_information	pbi;
	nt_thread_context		context;
	nt_user_stack			stack;
	nt_memory_basic_information	mbi;
	nt_client_id			cid;
	nt_large_integer		timeout;

	hprocess = hthread = (void *)0;

	oa.len		= sizeof(nt_object_attributes);
	oa.root_dir	= 0;
	oa.obj_name	= 0;
	oa.obj_attr	= 0;
	oa.sec_desc	= 0;
	oa.sec_qos	= 0;

	if ((status = __ntapi->zw_create_process(
			&hprocess,
			NT_PROCESS_ALL_ACCESS,
			&oa,
			NT_CURRENT_PROCESS_HANDLE,
			1,0,0,0)))
		return (intptr_t)(-1);

	if ((status = __ntapi->zw_query_information_process(
			hprocess,
			NT_PROCESS_BASIC_INFORMATION,
			(void *)&pbi,
			sizeof(nt_process_basic_information),
			0)))
		return __tt_fork_cancel(hprocess,status);



	__ntapi->tt_aligned_block_memset(
		&context,0,sizeof(nt_thread_context));

	__INIT_CONTEXT(context);
	context.STACK_POINTER_REGISTER = saved_regs_stack_pointer;
	context.FAST_CALL_ARG0 = saved_regs_stack_pointer;

	context.INSTRUCTION_POINTER_REGISTER = stack_adjustment
		? (uintptr_t)__tt_fork_child_entry_point_adj
		: (uintptr_t)__tt_fork_child_entry_point;



	if ((status = __ntapi->zw_query_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void *)context.STACK_POINTER_REGISTER,
			NT_MEMORY_BASIC_INFORMATION,
			&mbi,sizeof(nt_memory_basic_information),0)))
		return __tt_fork_cancel(hprocess,status);

	stack.fixed_stack_base		= (void *)0;
	stack.fixed_stack_limit		= (void *)0;
	stack.expandable_stack_base	= (void *)((uintptr_t)mbi.base_address + mbi.region_size);
	stack.expandable_stack_limit	= (void *)mbi.base_address;
	stack.expandable_stack_bottom	= (void *)mbi.allocation_base;



	__internals	= __ntapi_internals();
	hport_session	= &__internals->hport_tty_session;
	timeout.quad	= (-1) * 10 * 1000 * __NT_FORK_CHILD_WAIT_MILLISEC;

	if (hport_session && *hport_session)
		if ((status = __ntapi->tty_client_process_register(
				*hport_session,
				pbi.unique_process_id,
				0, 0, &timeout)))
			return __tt_fork_cancel(hprocess,status);


	if ((status = __ntapi->zw_create_thread(
			&hthread,
			NT_THREAD_ALL_ACCESS,
			&oa,hprocess,&cid,
			&context,&stack,0)))
		return __tt_fork_cancel(hprocess,status);


	if (cid.process_id > 0) {
		__internals->hany[0] = hprocess;
		__internals->hany[1] = hthread;
	} else {
		__internals->hany[0] = 0;
		__internals->hany[1] = 0;
	}

	/* hoppla */
	return (int32_t)cid.process_id;
}

intptr_t __fastcall __ntapi_tt_fork_v1(
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

	if (at_locked_cas(&__internals->hlock,0,1))
		return (intptr_t)(-1);

	if (hport_session && *hport_session)
		if (__ntapi_tt_create_inheritable_event(
				&hevent_tty_connected,
				NT_NOTIFICATION_EVENT,
				NT_EVENT_NOT_SIGNALED))
			return (intptr_t)(-1);

	pid = __tt_fork_v1();

	*hprocess = __internals->hany[0];
	*hthread  = __internals->hany[1];

	at_store(&__internals->hlock,0);

	if (hport_session && *hport_session) {
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

		} else if (pid > 0) {
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
	}

	return pid;
}
