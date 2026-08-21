/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/
/* tcc_posix (R5): __sys_select / __sys_pselect6 实现.
 *
 * 与 _poll.c 同款设计取舍: 合法已打开 fd 一律就绪; 非法 fd 不置位。
 * fd_set 布局与 musl <sys/select.h> 一致 (FD_SETSIZE=1024, FD_ZERO/FD_SET
 * 基于每字一位)。
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

#define PSX_FD_SETSIZE	1024
#define PSX_NFDBITS	(8 * (int)sizeof(unsigned long))

struct __psx_fdset {
	unsigned long	fds_bits[PSX_FD_SETSIZE / PSX_NFDBITS];
};

struct __psx_timeval {
	long	tv_sec;
	long	tv_usec;
};

struct __psx_timespec {
	long	tv_sec;
	long	tv_nsec;
};

#define __psx_fdisset(d,s) \
	((s)->fds_bits[(d)/PSX_NFDBITS] & (1UL << ((d)%PSX_NFDBITS)))
#define __psx_fdset(d,s) \
	((s)->fds_bits[(d)/PSX_NFDBITS] |= (1UL << ((d)%PSX_NFDBITS)))
#define __psx_fdclr(d,s) \
	((s)->fds_bits[(d)/PSX_NFDBITS] &= ~(1UL << ((d)%PSX_NFDBITS)))

static void __psx_select_delay(int micros)
{
	nt_large_integer delay;

	if (micros <= 0)
		return;

	delay.quad = (int64_t)micros * 10LL; /* us -> 100ns 单位 */
	__ntapi->zw_delay_execution(0,&delay);
}

__psx_api
intptr_t __sys_select(int n,
	struct __psx_fdset * rfds, struct __psx_fdset * wfds,
	struct __psx_fdset * efds, struct __psx_timeval * tv)
{
	struct __psx_tlca *	tlca;
	int			fd;
	intptr_t		ret;
	struct __psx_fdset	rimask, wimask, eimask;
	struct __psx_fdset *	rofd;
	struct __psx_fdset *	wofd;
	struct __psx_fdset *	eofd;
	struct __ofd *		ofd;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* 记录输入掩码, 输出集合先清零 */
	rofd = rfds; wofd = wfds; eofd = efds;

	if (rfds) { rimask = *rfds; memset(rfds,0,sizeof(*rfds)); }
	if (wfds) { wimask = *wfds; memset(wfds,0,sizeof(*wfds)); }
	if (efds) { eimask = *efds; memset(efds,0,sizeof(*efds)); }

	if (n > PSX_FD_SETSIZE)
		n = PSX_FD_SETSIZE;

	ret = 0;
	for (fd = 0; fd < n; fd++) {
		unsigned in = 0;

		if (rofd && __psx_fdisset(fd,&rimask)) in |= 1;
		if (wofd && __psx_fdisset(fd,&wimask)) in |= 2;
		if (eofd && __psx_fdisset(fd,&eimask)) in |= 4;

		if (!in)
			continue;

		/* 非法 fd: 不在任何输出集合置位 (跳过) */
		if (!(ofd = __psx_ofd_ref_inc(tlca->ctx,fd)))
			continue;

		/* 合法 fd: 一律就绪 → 在各自输出集合置位 */
		if ((in & 1) && rofd) { __psx_fdset(fd,rofd); }
		if ((in & 2) && wofd) { __psx_fdset(fd,wofd); }
		if ((in & 4) && eofd) { __psx_fdset(fd,eofd); }
		ret++;

		__psx_ofd_ref_dec(tlca->ctx,ofd);
	}

	if (!ret && tv)
		__psx_select_delay((int)(tv->tv_sec * 1000000L + tv->tv_usec));

	return __psx_sig_epilog(tlca,ret,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_pselect6(int n,
	struct __psx_fdset * rfds, struct __psx_fdset * wfds,
	struct __psx_fdset * efds, const struct __psx_timespec * tsp, const void * data)
{
	struct __psx_tlca *	tlca;
	struct __psx_timeval	tv;
	struct __psx_fdset	*r, *w, *e;

	(void)data;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	r = rfds; w = wfds; e = efds;

	if (tsp) {
		tv.tv_sec  = tsp->tv_sec;
		tv.tv_usec = tsp->tv_nsec / 1000;
		return __psx_sig_epilog(tlca,
			__sys_select(n,r,w,e,&tv),NT_STATUS_SUCCESS);
	}

	return __psx_sig_epilog(tlca,
		__sys_select(n,r,w,e,0),NT_STATUS_SUCCESS);
}