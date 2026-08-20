/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_session.h"
#include "psx_tlca.h"
#include "psx_helper.h"
#include "psx_errno.h"
#include "psx_debug.h"
#include "psx.h"

enum __wait_type {
	__WAIT_ALL,
	__WAIT_PID,
	__WAIT_PGID,
	__WAIT_POLL
} idtype_t;

enum __wait_target {
	__WAIT_PROCESS,
	__WAIT_EVENT,
	__WAIT_CAP
};

struct __wait_ctx {
	void *			handles[__WAIT_CAP];
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __process_record*rec;
	id_t			pid;
	siginfo_t *		info;
	struct __rusage *	rusage;
	int32_t			status;
	int32_t			waiter;
};

static int32_t __wait_no_more(struct __wait_ctx * waitctx)
{
	int32_t status;

	if ((status = __ntapi->zw_query_information_process(
			waitctx->rec->hprocess,
			NT_PROCESS_BASIC_INFORMATION,
			&waitctx->rec->pbi,sizeof(nt_pbi),0)))
		return status;

	__ntapi->sprintf((char *)waitctx->tlca->strace,
			"__wait_no_more: pid: %d: handle: 0x%08x: exitcode: 0x%08x: (waiter: %d)\n",
			waitctx->pid,
			waitctx->rec->hprocess,
			waitctx->rec->pbi.exit_status,
			waitctx->waiter);

	waitctx->tlca->wait_handle = waitctx->rec->hprocess;
	waitctx->tlca->wait_pid    = waitctx->rec->pid;
	waitctx->tlca->wait_rusage = waitctx->rusage;
	waitctx->tlca->wait_info   = waitctx->info;
	waitctx->tlca->wait_ecode  = waitctx->rec->pbi.exit_status;
	waitctx->tlca->wait_status = waitctx->status;

	if (waitctx->rusage)
		__ntapi->tt_aligned_block_memset(
			waitctx->rusage,	0,
			sizeof(*waitctx->rusage));

	return NT_STATUS_SUCCESS;
}


static int32_t __wait_for_one(void * rapunzel)
{
	int32_t			status;
	struct __wait_ctx *	waitctx;
	int32_t			test;
	struct __psx_tlca *	tlca;

	waitctx = (struct __wait_ctx *)rapunzel;
	waitctx->waiter = pe_get_current_thread_id();
	tlca = waitctx->tlca;

	status = __ntapi->zw_wait_for_multiple_objects(
		__WAIT_CAP,
		waitctx->handles,
		NT_WAIT_ANY,
		NT_SYNC_NON_ALERTABLE,
		0);

	__ntapi->sprintf((char *)tlca->strace,
		"__wait_for_one (either or): pid: %d: handle: 0x%08x: status: 0x%08x: (waiter: %d)\n",
		waitctx->pid,
		waitctx->handles[__WAIT_PROCESS],
		status,
		waitctx->waiter);

	status = __ntapi->zw_wait_for_single_object(
		waitctx->handles[__WAIT_PROCESS],
		NT_SYNC_NON_ALERTABLE,
		tlca->cfzerowait);

	__ntapi->sprintf((char *)tlca->strace,
		"__wait_for_one: pid: %d: handle: 0x%08x: status: 0x%08x: (waiter: %d)\n",
		waitctx->pid,
		waitctx->handles[__WAIT_PROCESS],
		status,
		waitctx->waiter);

	if (status == NT_STATUS_SUCCESS) {
		do {		
			test = at_locked_cas_32(
				&tlca->wait_status,
				NT_STATUS_PENDING,
				NT_STATUS_MORE_PROCESSING_REQUIRED);

			__ntapi->sprintf((char *)tlca->strace,
				"__wait_for_one: at_locked_cas_32: pid: %d: handle: 0x%08x: status: 0x%08x: test: 0x%08x: (waiter: %d)\n",
				waitctx->pid,
				waitctx->handles[__WAIT_PROCESS],
				status,test,
				waitctx->waiter);
		} while (test == NT_STATUS_MORE_PROCESSING_REQUIRED);

		if (test == NT_STATUS_PENDING)
			switch (at_locked_cas_32(&waitctx->rec->waitable,1,0)) {
				case 1:
					waitctx->status = NT_STATUS_SUCCESS;
					__wait_no_more(waitctx);
					break;
				default:
					at_locked_cas_32(
						&tlca->wait_status,
						NT_STATUS_MORE_PROCESSING_REQUIRED,
						NT_STATUS_PENDING);
			}
	}

	if ((test = at_locked_xsub_32(&tlca->wait_count,1) == 1))
		__ntapi->zw_alert_thread(tlca->hthread);

	return status;
}


static int32_t __wait_for_nobody(struct __wait_ctx * waitctx, int type)
{
	if (!(waitctx->status = __ntapi->zw_wait_for_single_object(
			waitctx->rec->hprocess,
			NT_SYNC_NON_ALERTABLE,
			waitctx->tlca->cfzerowait)))
		return NT_STATUS_SUCCESS;

	else if (waitctx->status != NT_STATUS_TIMEOUT)
		return waitctx->status;

	else if (type == __WAIT_POLL)
		return waitctx->status;

	waitctx->handles[__WAIT_PROCESS] = waitctx->rec->hprocess;
	waitctx->handles[__WAIT_EVENT]   = waitctx->tlca->hevent;

	at_locked_inc_32(&waitctx->tlca->wait_count);

	if ((waitctx->status = __psx_create_internal_thread(
			0,__wait_for_one,
			waitctx,sizeof(*waitctx))))
		at_locked_dec_32(&waitctx->tlca->wait_count);

	return waitctx->status;
}





static intptr_t __wait_in_vain(
	int			idtype,
	siginfo_t *		siginfo,
	int			options,
	struct __rusage *	rusage,
	int32_t *		exitcode,
	pid_t *			pid)
{
	int32_t				status;
	struct __psx_tlca *		tlca;
	struct __psx_ctx *		ctx;
	struct __process_record *	rec;
	struct __wait_ctx		waitctx;
	struct dalist_node_ex *		node;
	int32_t				pgid;
	intptr_t			ret;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_io_prolog(tlca);


	/* completion of previous round */
	while (tlca->wait_count);

	/* setup */
	tlca->wait_pid		= 0;
	tlca->wait_info		= 0;
	tlca->wait_rusage	= 0;
	tlca->wait_status	= NT_STATUS_PENDING;
	tlca->wait_ecode 	= 0;

	if (!tlca->hevent)
		__ntapi->tt_create_private_event(
			&tlca->hevent,
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED);
	else
		__ntapi->zw_reset_event(
			tlca->hevent,0);





	/* specific */
	if (*pid > 0) {
		if (!(rec = __psx_get_process_record(
				&ctx->offsprings,
				0,0,0,*pid,0)))
			return __psx_io_epilog(tlca,-ECHILD,NT_STATUS_NOT_FOUND);

		else if (!rec->waitable)
			return __psx_io_epilog(tlca,-ECHILD,NT_STATUS_RESOURCE_NOT_OWNED);

		waitctx.rec	= rec;
		waitctx.tlca	= tlca;
		waitctx.ctx	= ctx;
		waitctx.pid	= *pid;
		waitctx.info	= siginfo;
		waitctx.rusage	= rusage;

		if ((status = __wait_for_nobody(&waitctx,__WAIT_PID)))
			return __psx_io_epilog(tlca,-ECHILD,status);

		else if ((status = __wait_no_more(&waitctx)))
			return __psx_io_epilog(tlca,-ECHILD,status);

		else
			return __psx_io_epilog(tlca,0,NT_STATUS_SUCCESS);
	}




	/* semantics */
	if (*pid == 0)
		pgid = rtdata->alt_cid_self.pgid;

	else if (*pid == -1)
		pgid = -1;

	else if (*pid < -1)
		pgid = -(*pid);

	else
		pgid = 0;



	__psx_pid_lock_acquire();



	/* poll */
	node = (struct dalist_node_ex *)ctx->offsprings.head;

	for (; pgid && node; node=node->next) {
		rec  = (struct __process_record *)&node->dblock;
		*pid = rec->pid;


		__ntapi->sprintf((char *)tlca->strace,
			"__wait_in_vain: poll: pid: %d\n",
			rec->pid);
			

		if ((pgid == -1) || (rec->pgid == pgid))
			if (rec->waitable) {
				waitctx.rec	= rec;
				waitctx.tlca	= tlca;
				waitctx.ctx	= ctx;
				waitctx.pid	= rec->pid;
				waitctx.info	= siginfo;
				waitctx.rusage	= rusage;

			if (!(__wait_for_nobody(&waitctx,__WAIT_POLL)))
				if (!(__wait_no_more(&waitctx)))
					return __psx_io_epilog(
						tlca,0,
						__psx_pid_lock_release(0));
		}
	}



	/* pool */
	node = (struct dalist_node_ex *)ctx->offsprings.head;
	status = NT_STATUS_MORE_ENTRIES;

	for (; pgid && status && node; node=node->next) {
		rec = (struct __process_record *)&node->dblock;


		__ntapi->sprintf((char *)tlca->strace,
			"__wait_in_vain: pool: pid: %d\n",
			*pid);


		if ((pgid == -1) || (rec->pgid == pgid))
			if (rec->waitable) {
				waitctx.rec	= rec;
				waitctx.tlca	= tlca;
				waitctx.ctx	= ctx;
				waitctx.pid	= rec->pid;
				waitctx.info	= siginfo;
				waitctx.rusage	= rusage;

			__wait_for_nobody(&waitctx,__WAIT_PGID);
		}
	}



	__psx_pid_lock_release(0);






	/* wait in vain */
	status = __ntapi->zw_wait_for_single_object(
		tlca->hevent,
		NT_SYNC_ALERTABLE,
		0);

	while ((status == NT_STATUS_ALERTED) && tlca->wait_count && !tlca->sig_count)
		status = __ntapi->zw_wait_for_single_object(
			tlca->hevent,
			NT_SYNC_ALERTABLE,
			0);

	if ((status == NT_STATUS_ALERTED) && tlca->sig_count)
		__ntapi->zw_delay_execution(&tlca->cfalert,0);

	while ((status == NT_STATUS_ALERTED) && tlca->wait_count && tlca->frestart) {
		status = __ntapi->zw_wait_for_single_object(
			tlca->hevent,
			NT_SYNC_ALERTABLE,
			0);

		if ((status == NT_STATUS_ALERTED) && tlca->sig_count)
			__ntapi->zw_delay_execution(&tlca->cfalert,0);
	}






	/* status */
	if (tlca->wait_count)
		__ntapi->zw_set_event(tlca->hevent,0);

	while (tlca->wait_count);

	if (status == NT_STATUS_ALERTED)
		at_locked_cas_32(&tlca->wait_pid,0,-EINTR);

	if ((tlca->wait_pid > 0) && exitcode)
		*exitcode = tlca->wait_ecode << 8; /* Linux wait status 编码 */


	*pid = tlca->wait_pid;
	ret  = *pid ? 0 : -ECHILD;
	status = (tlca->wait_pid > 0)
		? tlca->wait_ecode
		: tlca->wait_status;

	return __psx_io_epilog(tlca,ret,status);
}



__psx_api
intptr_t __sys_wait4(pid_t pid, int * exitcode, int options, struct __rusage * rusage)
{
	intptr_t		ret;
	struct __psx_tlca *	tlca;

	tlca = __tlca_self();
	__ntapi->sprintf((char *)tlca->strace,
		"\n\n__sys_wait4: pid: %d: tlca: 0x%016x: waiting thread: %d\n",
		pid,tlca,pe_get_current_thread_id());


	ret = __wait_in_vain(__WAIT_PID,0,options,rusage,exitcode,&pid);

	if ((ret)) {
		return ret;
	}

	__ntapi->sprintf((char *)tlca->strace,
		"__sys_wait4: internal posix ret: %d: will return: %d: child exitcode: 0x%08x\n\n",
		ret,pid,
		ret ? -1 : tlca->exitcode);

	exitcode = exitcode ? exitcode : &tlca->exitcode;
	*exitcode = tlca->exitcode << 8; /* Linux wait status 编码 */
	return pid;
}

__psx_api
intptr_t __sys_waitid(int idtype, id_t id, siginfo_t * siginfo, int options)
{
	return __wait_in_vain(idtype,siginfo,options,0,0,(pid_t *)&id);
}
