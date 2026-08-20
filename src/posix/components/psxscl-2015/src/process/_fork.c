/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_impl.h"
#include "psx_systypes.h"
#include "psx_daemon.h"
#include "psx_init.h"
#include "psx_tlca.h"
#include "psx_session.h"
#include "psx_helper.h"
#include "psx_acl.h"
#include "psx_errno.h"
#include "psx_debug.h"
#include "psx.h"

struct __sigchld_ctx {
	struct __psx_ctx *	ctx;
	void *			hprocess;
	void *			hthread;
	nt_cid			cid;
};

static int __fork_dbg_dummy = 0;

static int __fork_dbg_helper(intptr_t ret)
{
	return !ret && __fork_dbg_dummy;
}

static intptr_t __fork_child_cancel(int32_t status)
{
	return __ntapi->zw_terminate_process(
		NT_CURRENT_PROCESS_HANDLE,
		status);
}

int32_t __fork_sigchld_wait(void * rapunzel)
{
	struct __sigchld_ctx *	sigctx;
	struct __port_msg	msg;

	sigctx = (struct __sigchld_ctx *)rapunzel;

	__ntapi->tt_aligned_block_memset(
		&msg.header,0,sizeof(msg.header));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg) - sizeof(msg.header);
	msg.header.msg_size	= sizeof(msg);
	msg.msginfo.opcode	= PSX_DAEMON_SIGCHLD;

	__ntapi->zw_wait_for_single_object(
		sigctx->hprocess,NT_SYNC_NON_ALERTABLE,0);

	return NT_STATUS_SUCCESS;/*below*/

	return __ntapi->zw_request_port(
		hport_internal_client,&msg);
}

intptr_t __sys_fork(void)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ofd;
	struct __sigchld_ctx	sigctx;
	struct __process_record*rec;
	nt_cid			cid;
	nt_oa			oa;
	intptr_t		ret;
	int32_t			i;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(rec = __psx_add_process_record(
			&ctx->offsprings,
			0,0,
			PSX_PROCESS_CHILD,
			0,0,0,0,
			rtdata->alt_cid_self.pgid,
			rtdata->alt_cid_self.sid)))
		return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);

	ret = __ntapi->tt_fork(
		&sigctx.hprocess,
		&sigctx.hthread);

	if (ret < 0)
		return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);

	else if (ret > 0) {
		__ntapi->zw_close(
			sigctx.hthread);

		sigctx.ctx		= ctx;
		sigctx.cid.process_id	= ret;
		sigctx.cid.thread_id	= 0;

		rec->hprocess		= sigctx.hprocess;
		rec->cid.process_id	= ret;
		rec->pid		= (int32_t)ret;
		rec->waitable		= 1;

		__psx_create_internal_thread(
			0,__fork_sigchld_wait,
			&sigctx,sizeof(sigctx));

		return __psx_sig_epilog(tlca,ret,NT_STATUS_SUCCESS);
	}





	while (__fork_dbg_helper(ret));





	hport_tty	= rtdata->hsession;
	tlca->hevent	= 0;

	oa.len		= sizeof(oa);
	oa.root_dir	= 0;
	oa.obj_name	= 0;
	oa.obj_attr	= 0;
	oa.sec_desc	= __PSX_DEF_SEC_DESC;
	oa.sec_qos	= __PSX_DEF_SEC_QOS;

	cid.process_id	= pe_get_current_process_id();
	cid.thread_id	= pe_get_current_thread_id();

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&rtdata->alt_cid_parent,
		(uintptr_t *)&rtdata->alt_cid_self,
		sizeof(rtdata->alt_cid_self));

	rtdata->alt_cid_self.pid = (int32_t)cid.process_id;


	if ((status = __ntapi->zw_open_thread(
			&tlca->hthread,
			NT_THREAD_ALL_ACCESS,
			&oa,&cid)))
		return status;




	if ((status = __psx_init_pgid()))
		return __fork_child_cancel(status);


	if ((status = __psx_daemon_init(0)))
		return __fork_child_cancel(status);

	if ((status = __psx_init_signal(&rtctx)))
		return __fork_child_cancel(status);

	if ((status = __psx_session_fork()))
		return status;









	for (i=0; i<ctx->ofd_cap; i++) {
		ofd = &ctx->ofd_slots[i];
		ofd->info.hevent = 0;

		if (ofd->info.fdtype == PSX_FD_PTY)
			if ((status = __ntapi->pty_reopen(hport_tty,ofd->info.hpty)))
				return __fork_child_cancel(status);
	}



	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
