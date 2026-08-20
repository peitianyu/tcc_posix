/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  tcc_posix: 2015 pre-alpha 未实现 tkill/kill (musl   */
/*  raise() 依赖 SYS_tkill, 缺失导致崩溃). 同步信号:    */
/*  本线程直接调 __sigvtbl 里注册的 handler.            */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include <pemagine/pemagine.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_sigfn.h"
#include "psx_signal.h"
#include "psx_impl.h"
#include "psx.h"

/* 同步投递: 调 __sigvtbl[signum] (sigaction 写入) */
static intptr_t __tcc_deliver(int signum)
{
	sigafn_t h = __sigvtbl[signum];

	if ((uintptr_t)h > 1UL) {           /* SIG_DFL=0, SIG_IGN=1 */
		h(signum, 0, 0);
		return 0;
	}
	if ((uintptr_t)h == 1UL)            /* SIG_IGN */
		return 0;
	/* SIG_DFL: 无法真正终止进程语义, 返回 EINVAL */
	return -EINVAL;
}

__psx_api
intptr_t __sys_tkill(int tid, int signum)
{
	struct __psx_tlca * tlca;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((signum < 0) || (signum >= 64))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	/* 仅支持投递给本线程 (raise 场景) */
	if (tid != (int)pe_get_current_thread_id())
		return __psx_sig_epilog(tlca,-ESRCH,EPSXONLY);

	return __psx_sig_epilog(tlca,__tcc_deliver(signum),NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_kill(int pid, int signum)
{
	struct __psx_tlca * tlca;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((signum < 0) || (signum >= 64))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	/* 仅支持投递给自身进程 (raise 等价) */
	if (pid != (int)pe_get_current_process_id())
		return __psx_sig_epilog(tlca,-ESRCH,EPSXONLY);

	return __psx_sig_epilog(tlca,__tcc_deliver(signum),NT_STATUS_SUCCESS);
}
