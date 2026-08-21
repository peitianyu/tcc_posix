/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  futex: busy-wait implementation (2015 pre-alpha     */
/*  has no kernel futex; psxscl threads are NT threads) */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/ntapi.h>
#include "psx_tlca.h"
#include "psx.h"

int __futex_waits;
void * __futex_last_addr;
int __futex_last_val;

#define __FUTEX_WAIT		0
#define __FUTEX_WAKE		1
#define __FUTEX_PRIVATE_FLAG	128

static void __futex_delay(void)
{
	/* tcc_posix: 用 SwitchToThread 让出 (zw_delay_execution 的
	   1ms 延迟会让 Windows 把唤醒线程排后, 新线程/等待线程
	   可能长时间饿死; SwitchToThread 明确让给就绪线程) */
	typedef int (__stdcall *swt_fn)(void);
	static swt_fn pSwitchToThread;
	if (!pSwitchToThread) {
		pSwitchToThread = (swt_fn)pe_get_procedure_address(
			pe_get_kernel32_module_handle(), "SwitchToThread");
	}
	if (pSwitchToThread)
		pSwitchToThread();
	else {
		nt_large_integer delay;
		delay.quad = 10000; /* 100ns units: 1ms */
		__ntapi->zw_delay_execution(0,&delay);
	}
}

__psx_api
intptr_t __sys_futex(
	int *		uaddr,
	int		op,
	int		val,
	void *		timeout,
	void *		uaddr2,
	int		val3)
{
	int cmd = op & 127;

	(void)uaddr2;
	(void)val3;

	{
		extern int __futex_waits;
		extern void *__futex_last_addr;
		extern int __futex_last_val;
		__futex_waits++;
		__futex_last_addr = uaddr;
		__futex_last_val = val;
	}

	switch (cmd) {
	case __FUTEX_WAIT:
		if (timeout) {
			struct timespec *	ts = (struct timespec *)timeout;
			intptr_t		deadline;
			intptr_t		now;

			/* 相对超时 (musl __timedwait 已换算) */
			deadline = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
			for (;;) {
				struct timespec cur;
				intptr_t elapsed;

				if (*(volatile int *)uaddr != val)
					return 0;
				__sys_clock_gettime(1 /*CLOCK_MONOTONIC*/,&cur);
				elapsed = cur.tv_sec * 1000 + cur.tv_nsec / 1000000;
				if (elapsed >= deadline)
					return -110;
				__futex_delay();
			}
		} else {
			while (*(volatile int *)uaddr == val)
				__futex_delay();
			return 0;
		}
	case __FUTEX_WAKE:
		return 0;
	}

	return -ENOSYS;
}

__psx_api
intptr_t __sys_set_robust_list(void * head, size_t len)
{
	(void)head;
	(void)len;
	return 0;
}
