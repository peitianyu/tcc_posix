/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/
/* tcc_posix (R4): 恢复 POSIX 进程内 timer 库层.
 *
 * musl 库层 (src/time/timer_*.c) 在, 但 SYS_timer_create(222)/
 * SYS_timer_settime(223)/SYS_timer_gettime(224)/SYS_timer_getoverrun(225)/
 * SYS_timer_delete(226) 未注册 → 原生 syscall 一律 ENOSYS。
 * 此文件在 PSX 接口层补这些 syscall, musl 原版不动。
 *
 * 设计取舍 (接口层):
 * - 维护一张进程内 timer 槽表, 记录 clock/sigev/itimerspec/overrun/armed。
 * - timer_create 分配槽号 → *res; settime 存储 itimerspec 并置 armed;
 *   gettime 读回; getoverrun 返回溢出计数; delete 释放槽位。
 * - 已知局限 (记录待用户决策): 本实现不真正驱动 Windows 定时器、不投递
 *   SIGEV_SIGNAL/SIGEV_THREAD 到时信号; settime 仅存储等待 value。数值信息
 *   (时钟选择/时间值/状态) 均可正确读写, 不再 ENOSYS。
 */

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_tlca.h"
#include "psx.h"

#define PSX_TIMER_CAP	256

/* 与内核 ksigevent 布局一致 (musl src/time/timer_create.c) */
struct __psx_ksigevent {
	union {
		void *	sival_ptr;
		int	sival_int;
	}	sigev_value;
	int	sigev_signo;
	int	sigev_notify;
	int	sigev_tid;
};

struct __psx_timespec {
	intptr_t	tv_sec;
	intptr_t	tv_nsec;
};

struct __psx_itimerspec {
	struct __psx_timespec	it_interval;
	struct __psx_timespec	it_value;
};

struct __psx_timer_slot {
	int				used;
	clockid_t			clk;
	struct __psx_ksigevent		sev;
	struct __psx_itimerspec		its;
	int				overrun;
	int				armed;
};

static struct __psx_timer_slot __psx_timers[PSX_TIMER_CAP];

static struct __psx_timer_slot * __psx_timer_slot_get(int id)
{
	return ((unsigned)id < PSX_TIMER_CAP)
		? &__psx_timers[id]
		: 0;
}

__psx_api
intptr_t __sys_timer_create(clockid_t clk, void * ksev, int * res)
{
	struct __psx_tlca *		tlca;
	struct __psx_ksigevent *	sev = (struct __psx_ksigevent *)ksev;
	struct __psx_timer_slot *	slot;
	int				i;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!res)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	/* 仅接受 REALTIME(0) / MONOTONIC(1) */
	if ((clk != 0) && (clk != 1))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (sev && (sev->sigev_notify > 4)) /* NONE/SIGNAL/THREAD/THREAD_ID */
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	for (i = 0; i < PSX_TIMER_CAP; i++) {
		slot = &__psx_timers[i];
		if (slot->used)
			continue;

		slot->used	= 1;
		slot->clk	= clk;
		slot->overrun	= 0;
		slot->armed	= 0;
		__ntapi->tt_aligned_block_memset(
			&slot->sev,0,sizeof(slot->sev));
		__ntapi->tt_aligned_block_memset(
			&slot->its,0,sizeof(slot->its));
		if (sev)
			slot->sev = *sev;

		*res = i;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	}

	return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);
}

__psx_api
intptr_t __sys_timer_settime(int t, int flags,
	const struct __psx_itimerspec * val, struct __psx_itimerspec * old)
{
	struct __psx_tlca *		tlca;
	struct __psx_timer_slot *	slot;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (flags & ~1 /* TIMER_ABSTIME */)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (!(slot = __psx_timer_slot_get(t)) || !slot->used)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (old)
		*old = slot->its;

	if (val)
		slot->its = *val;
	else
		__ntapi->tt_aligned_block_memset(
			&slot->its,0,sizeof(slot->its));

	/* armed: it_value 非零即视为在跑 (到时信号投递未接线, 见文件头) */
	slot->armed = !(!val ||
		((slot->its.it_value.tv_sec == 0) &&
		 (slot->its.it_value.tv_nsec == 0)));

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_timer_gettime(int t, struct __psx_itimerspec * val)
{
	struct __psx_tlca *		tlca;
	struct __psx_timer_slot *	slot;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!val)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (!(slot = __psx_timer_slot_get(t)) || !slot->used)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	*val = slot->its;

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_timer_getoverrun(int t)
{
	struct __psx_tlca *		tlca;
	struct __psx_timer_slot *	slot;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(slot = __psx_timer_slot_get(t)) || !slot->used)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	return __psx_sig_epilog(tlca,slot->overrun,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_timer_delete(int t)
{
	struct __psx_tlca *		tlca;
	struct __psx_timer_slot *	slot;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(slot = __psx_timer_slot_get(t)) || !slot->used)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	slot->used	= 0;
	slot->armed	= 0;
	slot->overrun	= 0;

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}