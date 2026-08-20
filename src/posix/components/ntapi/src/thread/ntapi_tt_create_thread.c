/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_memory.h>
#include <ntapi/nt_thread.h>
#include <ntapi/nt_process.h>
#include <ntapi/nt_string.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


/* (no planned support of alpha processors, use constant values) */
#define __PAGE_SIZE 		0x001000
#define __GRANULARITY		0x010000
#define __RESERVE_ROUND_UP	0x100000

static int32_t __stdcall __create_thread_fail(
	void *	hprocess,
	void *	stack_bottom,
	size_t	stack_size,
	int32_t status)
{
	__ntapi->zw_free_virtual_memory(
		hprocess,
		&stack_bottom,
		&stack_size,
		NT_MEM_RELEASE);
	return status;
}

int32_t __stdcall __ntapi_tt_create_thread(
	__in_out	nt_thread_params *	params)
{
	int32_t				status;
	ntapi_internals *		__internals;

	nt_client_id			cid;
	nt_port_message_csrss_process	csrss_msg;
	nt_port_message_csrss_process *	csrss_msg_1st;
	nt_port_message_csrss_thread *	csrss_msg_any;

	void *			stack_system_limit;
	uint32_t		protect_type_old;

	nt_user_stack		stack;
	nt_thread_context *	pcontext;
	uint8_t			ctxbuf[sizeof(nt_thread_context) + 16];
	uintptr_t		fsuspended;
	uintptr_t *		parg;

	/* tcc 0.9.28rc does not honor aligned(16) on locals: manual align */
	pcontext = (nt_thread_context *)((((uintptr_t)ctxbuf) + 15) & ~(uintptr_t)15);

	if (!(params->stack_size_commit))
		return NT_STATUS_INVALID_PARAMETER;
	else if (!(params->stack_size_reserve))
		return NT_STATUS_INVALID_PARAMETER;
	else if (params->ext_ctx_size > __NT_INTERNAL_PAGE_SIZE)
		return NT_STATUS_INVALID_PARAMETER;
	else if (params->ext_ctx_size % sizeof(intptr_t))
		return NT_STATUS_INVALID_PARAMETER;
	else if (params->arg && params->ext_ctx)
		return NT_STATUS_INVALID_PARAMETER_MIX;
	else if (params->ext_ctx && !params->ext_ctx_size)
		return NT_STATUS_INVALID_PARAMETER_MIX;

	/* init */
	__internals  = __ntapi_internals();
	params->stack_size_commit  = __NT_ROUND_UP_TO_POWER_OF_2(params->stack_size_commit+params->ext_ctx_size, __PAGE_SIZE);
	params->stack_size_reserve = __NT_ROUND_UP_TO_POWER_OF_2(params->stack_size_reserve,__GRANULARITY);

	/* compare, round-up as needed */
	if (params->stack_size_commit >= params->stack_size_reserve)
		params->stack_size_reserve = __NT_ROUND_UP_TO_POWER_OF_2(params->stack_size_commit,__RESERVE_ROUND_UP);

	/**
	  *
	  *  --------- BASE ----------
	  *
	  *  ---- (COMMITED AREA) ----
	  *
	  *  --------- LIMIT ---------
	  *
	  *  ------ GUARD PAGE -------
	  *
	  *  ------ ACTUAL LIMIT -----
	  *
	  *  ---- (RESERVED AREA) ----
	  *
	  *  -------- BOTTOM ---------
	  *
	**/

	/* stack structure: unused fields */
	stack.fixed_stack_base  = (void *)0;
	stack.fixed_stack_limit = (void *)0;

	/* first we reserve */
	stack.expandable_stack_bottom = (void *)0;
	status = __ntapi->zw_allocate_virtual_memory(
		params->hprocess,
		&stack.expandable_stack_bottom,
		params->stack_zero_bits,
		&params->stack_size_reserve,
		NT_MEM_RESERVE,
		NT_PAGE_READWRITE);

	if (status) return status;

	/* calculate base and limit */
	stack.expandable_stack_base =
		(void *)((intptr_t)stack.expandable_stack_bottom
			+ params->stack_size_reserve);

	stack.expandable_stack_limit =
		(void *)((intptr_t)stack.expandable_stack_base
			- params->stack_size_commit);

	/* guard page */
	params->stack_size_commit += __PAGE_SIZE;
	stack_system_limit =
		(void *)((intptr_t)stack.expandable_stack_base
			- params->stack_size_commit);

	/* then we commit */
	status = __ntapi->zw_allocate_virtual_memory(
		params->hprocess,
		&stack_system_limit,
		0,
		&params->stack_size_commit,
		NT_MEM_COMMIT,
		NT_PAGE_READWRITE);

	if (status) return __create_thread_fail(
		params->hprocess,
		stack.expandable_stack_bottom,
		params->stack_size_reserve,
		status);

	/* finally we protect the guard page */
	params->stack_size_commit = __PAGE_SIZE;
	status = __ntapi->zw_protect_virtual_memory(
		params->hprocess,
		&stack_system_limit,
		&params->stack_size_commit,
		NT_PAGE_READWRITE | NT_MEM_PAGE_GUARD,
		&protect_type_old);

	if (status) return __create_thread_fail(
		params->hprocess,
		stack.expandable_stack_bottom,
		params->stack_size_reserve,
		status);



	/* context */
	if (!params->reg_context) {
		params->reg_context = pcontext;
		__ntapi->tt_aligned_block_memset(pcontext,0,sizeof(nt_thread_context));
		__INIT_CONTEXT(pcontext[0]);
		pcontext->INSTRUCTION_POINTER_REGISTER = (uintptr_t)params->start;
		pcontext->STACK_POINTER_REGISTER = (uintptr_t)(stack.expandable_stack_base)
						- sizeof(intptr_t);
	}





/*****************************************************************************/
/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-*/
/*                                                                           */
/*                                                                           */
/*      INNOVATION IN THE FIELD OF MULTI-THREADED COMPUTER PROGRAMMING       */
/*                                                                           */
/*      A "RAPUNZEL" TOP-OF-STACK, VARIABLE-SIZE ENTRY-ROUTINE CONTEXT       */
/*                                                                           */
/*                   COPYRIGHT (C) 2013,2014,2015 ZVI GILBOA                 */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*      Laß mir dein Haar herunter.«                                         */
/**/	if (params->ext_ctx) {                                             /**/
/**/		pcontext->STACK_POINTER_REGISTER -= params->ext_ctx_size;    /**/
/**/		params->arg = (void *)pcontext->STACK_POINTER_REGISTER;      /**/
/**/                                                                       /**/
/**/		if (params->creation_flags & NT_CREATE_LOCAL_THREAD)       /**/
/**/			__ntapi->tt_aligned_block_memcpy(                  /**/
/**/				(uintptr_t *)params->arg,                  /**/
/**/				(uintptr_t *)params->ext_ctx,              /**/
/**/				params->ext_ctx_size);                     /**/
/**/		else {                                                     /**/
/**/			status = __ntapi->zw_write_virtual_memory(         /**/
/**/				params->hprocess,                          /**/
/**/				params->arg,                               /**/
/**/				(char *)params->ext_ctx,                   /**/
/**/				params->ext_ctx_size,                      /**/
/**/				0);                                        /**/
/**/                                                                       /**/
/**/			if (status) return __create_thread_fail(           /**/
/**/				params->hprocess,                          /**/
/**/				stack.expandable_stack_bottom,             /**/
/**/				params->stack_size_reserve,                /**/
/**/				status);                                   /**/
/**/		}                                                          /**/
/**/	}                                                                  /**/
/**/                                                                       /**/
/**/                                                                       /**/
/**/                                                                       /**/
/*      entry-routine argument address and stack pointer adjustment          */
/**/	if (sizeof(intptr_t) == 4) {                                       /**/
/**/		pcontext->STACK_POINTER_REGISTER -= sizeof(intptr_t);        /**/
/**/		parg  = (uintptr_t *)pcontext->STACK_POINTER_REGISTER;       /**/
/**/	} else                                                             /**/
/**/		parg  = &pcontext->FAST_CALL_ARG0;                           /**/
/**/                                                                       /**/
/**/                                                                       /**/
/*      write entry-routine argument                                         */
/**/	if ((sizeof(size_t) == 8)                                          /**/
/**/			|| (params->creation_flags&NT_CREATE_LOCAL_THREAD))/**/
/**/		*parg = (uintptr_t)params->arg;                            /**/
/**/	else {                                                             /**/
/**/		status = __ntapi->zw_write_virtual_memory(                 /**/
/**/			params->hprocess,                                  /**/
/**/			parg,                                              /**/
/**/			(char *)&params->arg,                              /**/
/**/			sizeof(uintptr_t),                                 /**/
/**/			0);                                                /**/
/**/                                                                       /**/
/**/		if (status) return __create_thread_fail(                   /**/
/**/			params->hprocess,                                  /**/
/**/			stack.expandable_stack_bottom,                     /**/
/**/			params->stack_size_reserve,                        /**/
/**/			status);                                           /**/
/**/	}                                                                  /**/
/**/                                                                       /**/
/**/                                                                       /**/
/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-*/
/*****************************************************************************/











	/* create thread */
	if ((!__ntapi->zw_create_user_process) | (params->creation_flags & NT_CREATE_SUSPENDED))
		fsuspended = 1;
	else
		fsuspended = 0;

	status = __ntapi->zw_create_thread(
		&params->hthread,
		NT_THREAD_ALL_ACCESS,
		params->obj_attr,
		params->hprocess,
		&cid,
		params->reg_context,
		&stack,
		fsuspended);

	if (status) return __create_thread_fail(
		params->hprocess,
		stack.expandable_stack_bottom,
		params->stack_size_reserve,
		status);

	/* for os versions prior to hasta la */
	if (!__ntapi->zw_create_user_process) {
		__ntapi->tt_aligned_block_memset(&csrss_msg,0,sizeof(csrss_msg));

		if (params->creation_flags & NT_CREATE_FIRST_THREAD_OF_PROCESS) {
			/* nt_port_message_csrss_process is the larger structure */
			csrss_msg_1st = &csrss_msg;

			csrss_msg_1st->header.data_size		= sizeof(nt_port_message_csrss_process) - sizeof(nt_port_message);
			csrss_msg_1st->header.msg_size		= sizeof(nt_port_message_csrss_process);
			csrss_msg_1st->opcode			= 0x10000;
			csrss_msg_1st->hprocess			= params->hprocess;
			csrss_msg_1st->hthread			= params->hthread;
			csrss_msg_1st->unique_process_id	= cid.process_id;
			csrss_msg_1st->unique_thread_id		= cid.thread_id;
		} else {
			/* nt_port_message_csrss_thread is the smaller structure */
			csrss_msg_any = (nt_port_message_csrss_thread *)&csrss_msg;

			csrss_msg_any->header.data_size		= sizeof(nt_port_message_csrss_thread) - sizeof(nt_port_message);
			csrss_msg_any->header.msg_size		= sizeof(nt_port_message_csrss_thread);
			csrss_msg_any->opcode			= 0x10001;
			csrss_msg_any->hthread			= params->hthread;
			csrss_msg_any->unique_process_id	= cid.process_id;
			csrss_msg_any->unique_thread_id		= cid.thread_id;
		}

		/* send csrss a new-thread notification */
		if (__internals->csr_port_handle_addr) {
			status = __ntapi->zw_request_wait_reply_port(
				*__internals->csr_port_handle_addr,
				&csrss_msg,&csrss_msg);
		}

		/* output csrss_status to caller */
		params->csrss_status = status
			? status
			: csrss_msg.status;
	}

	/* resume thread, close handle as needed */
	if (fsuspended && !(params->creation_flags & NT_CREATE_SUSPENDED))
		status = __ntapi->zw_resume_thread(params->hthread,0);

	if (params->creation_flags & NT_CLOSE_THREAD_HANDLE)
		__ntapi->zw_close(params->hthread);

	/* and finally */
	params->thread_id = (uint32_t)cid.thread_id;
	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_create_local_thread(
	__in_out	nt_thread_params *	params)
{
	void *				image_base;
	struct pe_stack_heap_info	stack_heap_info;
	nt_client_id			cid;
	nt_object_attributes		oa;
	nt_status			status;

        /* oa init */
	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= (nt_unicode_string *)0;
	oa.obj_attr	= 0;
	oa.sec_desc	= (nt_sd *)0;
	oa.sec_qos	= (nt_sqos *)0;

        /* init cid */
        cid.process_id = pe_get_current_process_id();
        cid.thread_id  = pe_get_current_thread_id();

	/* obtain a handle to our own process */
	/* TODO: use cached handle, no close  */
	status = __ntapi->zw_open_process(
		&params->hprocess,
		NT_PROCESS_ALL_ACCESS,
		&oa,
		&cid);

	if (status) return status;

	/* retrieve the stack defaults as needed */
	if (!(params->stack_size_commit && params->stack_size_reserve) && !(params->stack_info)) {
		/* image_base*/
		image_base = pe_get_first_module_handle();

		if (!(intptr_t)image_base)
			return NT_STATUS_INVALID_IMPORT_OF_NON_DLL;

		status = pe_get_image_stack_heap_info(
			image_base,
			&stack_heap_info);

		if (status)
			return NT_STATUS_INVALID_IMAGE_FORMAT;

		/* stack_size_commit */
		if (!params->stack_size_commit)
			params->stack_size_commit = stack_heap_info.size_of_stack_commit;

		/* stack_size_reserve */
		if (!params->stack_size_reserve)
			params->stack_size_reserve = stack_heap_info.size_of_stack_reserve;

		if (!(params->stack_size_commit && params->stack_size_reserve))
			return NT_STATUS_INVALID_IMAGE_FORMAT;
	}

	params->creation_flags |= NT_CREATE_LOCAL_THREAD;
	status =  __ntapi_tt_create_thread(params);

	/* TODO: use cached handle, no close  */
	__ntapi->zw_close(params->hprocess);
	return status;
}


int32_t __stdcall __ntapi_tt_create_remote_thread(
	__in_out	nt_thread_params *	params)
{
	return __ntapi_tt_create_thread(params);
}


void * __cdecl __ntapi_csr_port_handle(nt_status * pstatus)
{
	ntapi_internals * __internals;

	__internals = __ntapi_internals();

	if (__internals->csr_port_handle_addr) {
		if (pstatus)
			*pstatus = NT_STATUS_SUCCESS;
		return *__internals->csr_port_handle_addr;
	} else {
		if (pstatus)
			*pstatus = NT_STATUS_UNSUCCESSFUL;
		return (void *)0;
	}
}
