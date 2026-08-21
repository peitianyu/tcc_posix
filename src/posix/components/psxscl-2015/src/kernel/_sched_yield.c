/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  sched_yield: 让出当前线程剩余时间片                 */
/*  (2015 pre-alpha 未注册 → __sysvtbl[24]==NULL;       */
/*   R1 保护前直接段错误, 现改用 kernel32 SwitchToThread)*/
/********************************************************/

#include <pemagine/pemagine.h>
#include "psx.h"

__psx_api
intptr_t __sys_sched_yield(void)
{
	typedef int (__stdcall *swt_fn)(void);
	static swt_fn pSwitchToThread;

	/* SwitchToThread: 让出剩余时间片给同优先级别线程,
	   无就绪线程时立即返回 (非阻塞) */
	if (!pSwitchToThread) {
		pSwitchToThread = (swt_fn)pe_get_procedure_address(
			pe_get_kernel32_module_handle(), "SwitchToThread");
	}
	if (pSwitchToThread)
		pSwitchToThread();

	return 0;
}