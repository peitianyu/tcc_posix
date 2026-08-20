/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_daemon.h"
#include "psx_tlca.h"
#include "psx.h"

/* strace hooks removed */

static void __do_the_thing(void)
{
	struct __ucontext *	uctx;
	struct __psx_tlca *	tlca;

	tlca = __tlca_self();
	uctx = tlca->sig_ucontext;

	#ifdef PSX_DEBUG_IN_THE_DARK
	{
		nt_iosb iosb;

		__ntapi->zw_write_file(
			rtdata->hstderr,
			0,0,0,
			&iosb,"\n\n__do_the_thing (function)\n",
			28,0,0);
	}
	#endif

	/* debug removed: strace signal trampoline hooks */

	__ntapi->zw_delay_execution(
		&tlca->cfalert,
		tlca->cfzerowait);

	__ntapi->zw_continue(&uctx->uc_mcontext,0);
}

static void __stdcall __psx_signal_apc(int signum, siginfo_t * info, void * ctx)
{
	siginfo_t siginfo;
	size_t    infolen;

	#ifdef PSX_DEBUG_IN_THE_DARK
	{
		nt_iosb iosb;

		__ntapi->zw_write_file(
			rtdata->hstderr,
			0,0,0,
			&iosb,"__psx_signal_apc\n",
			17,0,0);
	}
	#endif

	infolen = __PSX_VIRTUAL_PAGE_SIZE;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&siginfo,
		(uintptr_t *)info,
		sizeof(siginfo));

	__ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		(void **)&info,&infolen,NT_MEM_RELEASE);

	__sigvtbl[signum](siginfo.si_signo,&siginfo,ctx);
}

int32_t __stdcall __psx_daemon_sigdeliver(struct __sig_msg * msg)
{
	int32_t			status;
	uintptr_t		rsp;
	void *			hthread;
	siginfo_t *		siginfo;
	size_t			infolen;
	struct __ucontext	uctx;
	nt_oa			oa = {sizeof(oa)};

	siginfo = 0;
	infolen = __PSX_VIRTUAL_PAGE_SIZE;
	hthread = msg->tlca->hthread;

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&siginfo,0,&infolen,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE)))
		return status;

	if ((status = __ntapi->zw_suspend_thread(hthread,0)))
		return status;

	__ntapi->tt_aligned_block_memset(
		&uctx,0,sizeof(uctx));

	uctx.uc_mcontext.uc_context_flags = NT_CONTEXT_JUST_EVERYTHING;

	if ((status = __ntapi->zw_get_context_thread(
			hthread,
			&uctx.uc_mcontext)))
		return status;

	rsp =  uctx.uc_mcontext.uc_rsp - sizeof(uctx);
	rsp -= uctx.uc_mcontext.uc_rsp % __PSX_UCONTEXT_ALIGN;
	msg->tlca->sig_ucontext = (struct __ucontext *)rsp;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)(msg->tlca->sig_ucontext),
		(uintptr_t *)&uctx,
		sizeof(uctx));

	/* abi(+), trampoline, guard */
	uctx.uc_mcontext.uc_rsp = rsp - __PSX_UCONTEXT_ALIGN;
	uctx.uc_mcontext.uc_rip = (uintptr_t)__do_the_thing;
	*(uintptr_t *)uctx.uc_mcontext.uc_rsp = -1;

	siginfo->si_signo = msg->sigtype;

	if ((status = __ntapi->zw_queue_apc_thread(
			hthread,
			(nt_knormal_routine *)__psx_signal_apc,
			(void *)msg->sigtype,siginfo,0)))
		return status;

	if ((status = __ntapi->zw_set_context_thread(
			hthread,&uctx.uc_mcontext)))
		return status;

	#ifdef PSX_DEBUG_IN_THE_DARK
	{
		nt_iosb iosb;

		__ntapi->zw_write_file(
			rtdata->hstderr,
			0,0,0,
			&iosb,"\n\n__do_the_thing (context)\n",
			27,0,0);
	}
	#endif

	if ((status = __ntapi->zw_resume_thread(
			hthread,0)))
		return status;

	return 0;
};
