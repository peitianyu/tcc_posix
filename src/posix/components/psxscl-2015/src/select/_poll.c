/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/
/* tcc_posix (R5): __sys_poll / __sys_ppoll 实现.
 *
 * 设计取舍 (接口层, musl 原版不动):
 * - 对每个打开的 fd 做最简就绪判定: 合法 fd 视作可读可写 (保持 musl 调用方
 *   语义不阻塞; 避免此处进入阻塞/超时态导致挂起). 非法 fd → POLLNVAL.
 * - 事件位掩码与 <poll.h> 对齐 (本头仅定义本文件需要的位, 不引入 poll.h).
 * - timeout 仅作为兜底: 当没有任何可报告 fd 且 timeout>0 时睡眠一次近似等待.
 */

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_tlca.h"
#include "psx_signal.h"
#include "psx_ofd.h"
#include "psx_impl.h"
#include "psx.h"

/* poll event bits (musl <poll.h> 对齐) */
#define PSX_POLLIN     0x001
#define PSX_POLLPRI    0x002
#define PSX_POLLOUT    0x004
#define PSX_POLLERR    0x008
#define PSX_POLLHUP    0x010
#define PSX_POLLNVAL   0x020

/* struct pollfd (musl <poll.h> 布局: int fd, short events, short revents) */
struct __psx_pollfd {
	int	fd;
	short	events;
	short	revents;
};

typedef unsigned long __psx_nfds_t;

/* 任取一个可报告事件位 (IN|PRI|OUT) */
#define PSX_POLL_RW  (PSX_POLLIN|PSX_POLLPRI|PSX_POLLOUT)

static short __psx_poll_revents(struct __psx_tlca * tlca, int fd, short events)
{
	struct __psx_ctx *	ctx;
	struct __ofd *		ofd;
	short			rev;

	ctx = tlca->ctx;
	rev = 0;

	if (!(ofd = __psx_ofd_ref_inc(ctx,fd)))
		return PSX_POLLNVAL;

	/* 合法已打开 fd: 本次实现一律可读可写 (见文件头设计取舍) */
	if (events & (PSX_POLLIN|PSX_POLLPRI|PSX_POLLOUT))
		rev = (short)(events & PSX_POLL_RW);

	__psx_ofd_ref_dec(ctx,ofd);
	return rev;
}

/* 睡眠约 timeout 毫秒的兜底 (当无可报告 fd 且 timeout>0) */
static void __psx_poll_delay(struct __psx_tlca * tlca, int timeout)
{
	nt_large_integer delay;

	if (timeout <= 0)
		return;

	delay.quad = (int64_t)timeout * 10000LL; /* ms -> 100ns 单位 */
	__ntapi->zw_delay_execution(0,&delay);
	(void)tlca;
}

__psx_api
intptr_t __sys_poll(struct __psx_pollfd * fds, __psx_nfds_t n, int timeout)
{
	struct __psx_tlca *	tlca;
	__psx_nfds_t		i;
	intptr_t		ret;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!fds || !n) {
		__psx_poll_delay(tlca,timeout);
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	}

	ret = 0;
	for (i = 0; i < n; i++) {
		if (!fds[i].events)
			continue;

		fds[i].revents = __psx_poll_revents(tlca,fds[i].fd,fds[i].events);
		if (fds[i].revents)
			ret++;
	}

	/* 全部事件位都问过但仍无就绪且给超时 → 近似睡眠一次 */
	if (!ret && (timeout > 0))
		__psx_poll_delay(tlca,timeout);

	return __psx_sig_epilog(tlca,ret,NT_STATUS_SUCCESS);
}

/* ppoll: 忽略 sigmask 语义 (本移植 pending 处理走 __psx_sig_*), 复用 poll 逻辑 */
__psx_api
intptr_t __sys_ppoll(struct __psx_pollfd * fds, __psx_nfds_t n, const void * tsp, const void * mask, size_t masksize)
{
	int timeout = -1;

	(void)mask;
	(void)masksize;

	if (tsp) {
		const long * ts = (const long *)tsp;
		timeout = (int)ts[0] * 1000 + (int)(ts[1] / 1000000);
	}

	return __sys_poll(fds,n,timeout);
}