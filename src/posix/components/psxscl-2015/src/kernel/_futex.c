/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  futex: kernel32 WaitOnAddress 真阻塞实现            */
/*  (2015 pre-alpha 原版是忙等; WaitOnAddress 内核原子   */
/*   比较+挂起, 语义与 futex 一致, 无唤醒丢失)          */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/ntapi.h>
#include "psx_tlca.h"
#include "psx.h"

#define __FUTEX_WAIT		0
#define __FUTEX_WAKE		1
#define __FUTEX_PRIVATE_FLAG	128
#define __FUTEX_INFINITE	0xFFFFFFFFu

static void __futex_delay(void)
{
	/* 忙等 fallback (WaitOnAddress 不可用时的老路径) */
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

/* ntdll RtlWaitOnAddress: 内核原子比较 *uaddr==val 则挂起, 否则立即返回。
   返回 NTSTATUS: 0=被唤醒/值改变, 0x102=超时。
   注意: 唤醒返回时值可能仍未变 (唤醒风暴), 调用者必须循环重试,
   不能"唤醒后检查值" — musl futex WAIT 语义是被唤醒即返回 0。 */
static int32_t __futex_wait_woa(int *uaddr, int val, uint32_t ms)
{
	typedef int32_t (__stdcall *rwoa_fn)(
		volatile void *, volatile void *, size_t, int64_t *);
	static rwoa_fn pRtlWaitOnAddress;
	int64_t rel100ns;

	if (!pRtlWaitOnAddress) {
		pRtlWaitOnAddress = (rwoa_fn)pe_get_procedure_address(
			pe_get_ntdll_module_handle(), "RtlWaitOnAddress");
	}
	if (!pRtlWaitOnAddress)
		return -1;

	if (ms == __FUTEX_INFINITE)
		rel100ns = 0; /* 0 = 无限等待 (RtlWaitOnAddress 语义) */
	else
		rel100ns = -(int64_t)ms * 10000; /* 负值 = 相对 100ns */

	return pRtlWaitOnAddress((volatile void *)uaddr,
		(volatile void *)&val, sizeof(int),
		ms == __FUTEX_INFINITE ? 0 : &rel100ns);
}

/* ntdll RtlWakeAddressSingle: 唤醒一个等待该地址的线程;
   无等待者时立即返回 (不会阻塞) */
static void __futex_wake_woa(int *uaddr)
{
	typedef void (__stdcall *rwba_fn)(volatile void *);
	static rwba_fn pRtlWakeAddressSingle;

	if (!pRtlWakeAddressSingle) {
		pRtlWakeAddressSingle = (rwba_fn)pe_get_procedure_address(
			pe_get_ntdll_module_handle(), "RtlWakeAddressSingle");
	}
	if (pRtlWakeAddressSingle)
		pRtlWakeAddressSingle((volatile void *)uaddr);
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

	switch (cmd) {
	case __FUTEX_WAIT: {
		uint32_t ms = __FUTEX_INFINITE;
		int32_t st;

		if (timeout) {
			struct timespec *	ts = (struct timespec *)timeout;

			/* musl __timedwait 已换算为相对超时 (ms 粒度) */
			ms = (uint32_t)(ts->tv_sec * 1000
				+ ts->tv_nsec / 1000000);
			if (*(volatile int *)uaddr != val)
				return 0;
		}

		st = __futex_wait_woa(uaddr, val, ms);
		if (st >= 0) {
			/* 0 = 被唤醒 (值可能未变, 调用者循环重试),
			   0x102 = STATUS_TIMEOUT */
			return st == 0x102 ? -110 : 0;
		}

		/* fallback: 忙等 (老系统无 RtlWaitOnAddress) */
		if (timeout) {
			intptr_t		deadline;
			for (;;) {
				struct timespec cur;
				intptr_t elapsed;

				if (*(volatile int *)uaddr != val)
					return 0;
				__sys_clock_gettime(1 /*CLOCK_MONOTONIC*/,&cur);
				elapsed = cur.tv_sec * 1000 + cur.tv_nsec / 1000000;
				if (elapsed >= (intptr_t)ms)
					return -110;
				__futex_delay();
			}
		} else {
			while (*(volatile int *)uaddr == val)
				__futex_delay();
			return 0;
		}
	}
	case __FUTEX_WAKE: {
		int n = val > 0 ? val : 1;

		/* WakeByAddressSingle 一次唤醒一个; 无等待者立即返回 */
		while (n--)
			__futex_wake_woa(uaddr);
		return 0;
	}
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
