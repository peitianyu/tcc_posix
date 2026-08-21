/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/
/* tcc_posix (R10a): 补全 POSIX mq (消息队列) syscall.
 *
 * musl 库层 (src/mq/mq_*.c) 在, 但 SYS_mq_open(240)/mq_unlink(241)/
 * mq_timedsend(242)/mq_timedreceive(243)/mq_notify(244)/mq_getsetattr(245)
 * 未注册 → 原生 syscall 一律 ENOSYS。此文件在 PSX 接口层补这些 syscall,
 * musl 原版不动。
 *
 * 设计取舍 (接口层, 静态槽表, 不递归 musl malloc):
 * - 队列: 进程内静态槽表 PSX_MQ_CAP 个, 每队列固定 PSX_MQ_SLOTS 个消息槽,
 *   每消息固定 PSX_MQ_MSGSIZE 字节。收到的 mq_maxmsg/mq_msgsize 超上限
 *   则截断 (不报错), 满足常规用法与测试。
 * - mqd 是 psxscl 真实 fd: mq_open 分配 fd+ofd, fdtype 复用 PSX_FD_OS_CONFIG
 *   (其 free/close 对 hfile==0 是安全空操作), mq 私有指针存 ofd->info.vfd;
 *   mq_close 直接走既有 SYS_close 路径释放。
 * - 阻塞语义: send/receive 满/空时轮询 + 短延时 (与 _select.c 同款设计);
 *   有 at (绝对 CLOCK_REALTIME) 时按截止时刻判定 ETIMEDOUT, at==0 则
 *   无限阻塞 (POSIX 语义)。
 * - mq_notify: 只登记/清除通知参数 (不真正投递信号), SIGEV_NONE 可用;
 *   SIGEV_THREAD 的 musl 用户态路径依赖 AF_NETLINK socket, nt64 上本就不通。
 * - mq_unlink: 标记槽未用 (后续 open 可复用), 已打开 fd 仍可访问该槽内存。
 *
 * 已知局限 (记录待用户决策): 无跨进程队列 (仅进程内), 通知不投递,
 * 队列槽仅在 unlink 时回收。
 */

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_fcntl.h"
#include "psx_tlca.h"
#include "psx_ofd.h"
#include "psx_signal.h"
#include "psx_impl.h"
#include "psx.h"

#define PSX_MQ_CAP	64
#define PSX_MQ_SLOTS	8
#define PSX_MQ_MSGSIZE	1024

#define PSX_MQ_PRIO_MAX	32768
#define PSX_MQ_DEF_MAXMSG	10
#define PSX_MQ_DEF_MSGSIZE	8192

#define PSX_MQ_SIGEV_NONE	0
#define PSX_MQ_SIGEV_SIGNAL	1
#define PSX_MQ_SIGEV_THREAD	2
#define PSX_MQ_SIGEV_THREAD_ID	4

struct __psx_mq_msg {
	uint32_t		used;
	uint32_t		seq;	/* 插入序号 (同优先级 FIFO) */
	uint32_t		prio;
	uint32_t		len;
	unsigned char		data[PSX_MQ_MSGSIZE];
};

struct __psx_mq {
	uint32_t		used;
	uint32_t		maxmsg;
	uint32_t		msgsize;
	uint32_t		curmsgs;
	uint32_t		flags;	/* O_NONBLOCK 等 (mq_flags) */
	uint32_t		next_seq;
	char			name[64];
	struct __psx_mq_msg	msg[PSX_MQ_SLOTS];
	/* mq_notify 登记 (仅存储, 不投递) */
	int			notify_signo;
	int			notify_notify;
};

static struct __psx_mq __psx_mqs[PSX_MQ_CAP];

static int __psx_mq_strcmp(const char * a, const char * b)
{
	while (*a && (*a == *b))
		a++, b++;

	return (unsigned char)*a - (unsigned char)*b;
}

static void __psx_mq_strncpy(char * dst, const char * src, size_t n)
{
	size_t i;

	for (i = 0; (i + 1 < n) && src[i]; i++)
		dst[i] = src[i];
	dst[i] = 0;
}

static struct __psx_mq * __psx_mq_lookup(const unsigned char * name)
{
	int i;

	for (i = 0; i < PSX_MQ_CAP; i++)
		if (__psx_mqs[i].used &&
			(__psx_mq_strcmp(__psx_mqs[i].name, (const char *)name) == 0))
			return &__psx_mqs[i];

	return 0;
}

static struct __psx_mq * __psx_mq_create(
	const unsigned char *	name,
	const void *		kattr)
{
	int			i;
	uint32_t		maxmsg, msgsize;
	struct __psx_mq *	mq;

	for (i = 0; i < PSX_MQ_CAP; i++)
		if (!__psx_mqs[i].used)
			break;

	if (i >= PSX_MQ_CAP)
		return 0;

	/* struct mq_attr { long flags,maxmsg,msgsize,curmsgs,... } */
	maxmsg	= PSX_MQ_DEF_MAXMSG;
	msgsize	= PSX_MQ_DEF_MSGSIZE;
	if (kattr) {
		const long * p = (const long *)kattr;
		if (p[1] > 0) maxmsg = (uint32_t)p[1];
		if (p[2] > 0) msgsize = (uint32_t)p[2];
	}

	mq = &__psx_mqs[i];
	mq->used	= 1;
	mq->maxmsg	= (maxmsg < PSX_MQ_SLOTS) ? maxmsg : PSX_MQ_SLOTS;
	mq->msgsize	= (msgsize < PSX_MQ_MSGSIZE) ? msgsize : PSX_MQ_MSGSIZE;
	mq->curmsgs	= 0;
	mq->flags	= 0;
	mq->next_seq	= 0;
	mq->notify_signo	= 0;
	mq->notify_notify	= PSX_MQ_SIGEV_NONE;
	__ntapi->tt_aligned_block_memset(
		&mq->msg,0,sizeof(mq->msg));
	__psx_mq_strncpy(mq->name,(const char *)name,sizeof(mq->name));

	return mq;
}

/* mqd (真实 fd) → mq 对象; 校验 fdtype==CONFIG 且 vfd 落在槽表内 */
static struct __psx_mq * __psx_mq_from_fd(
	struct __psx_ctx *	ctx,
	int			mqd)
{
	struct __ofd *		ofd;
	struct __psx_mq *	mq;

	if (!(ofd = __psx_ofd_ref_inc(ctx,mqd)))
		return 0;

	mq = (struct __psx_mq *)ofd->info.vfd;

	if ((mq < __psx_mqs) || (mq >= &__psx_mqs[PSX_MQ_CAP]))
		mq = 0;

	__psx_ofd_ref_dec(ctx,ofd);

	return mq;
}

static void __psx_mq_delay(int ms)
{
	nt_large_integer delay;

	delay.quad = (int64_t)ms * 10000LL; /* ms -> 100ns */
	__ntapi->zw_delay_execution(0,&delay);
}

static int __psx_mq_deadline_reached(const struct timespec * at)
{
	struct timespec now;

	__sys_clock_gettime(0 /*CLOCK_REALTIME*/,&now);

	return (now.tv_sec > at->tv_sec) ||
		((now.tv_sec == at->tv_sec) && (now.tv_nsec >= at->tv_nsec));
}

/* 发送: 0 成功 / 负 errno */
static int __psx_mq_send_locked(
	struct __psx_mq *	mq,
	const unsigned char *	msg,
	size_t			len,
	unsigned		prio)
{
	int			i;
	struct __psx_mq_msg *	slot;

	if (len > (size_t)mq->msgsize)
		return -EMSGSIZE;

	if (mq->curmsgs >= mq->maxmsg)
		return -EAGAIN;

	for (i = 0; i < PSX_MQ_SLOTS; i++)
		if (!mq->msg[i].used)
			break;

	if (i >= PSX_MQ_SLOTS)
		return -EAGAIN;

	slot = &mq->msg[i];
	slot->used	= 1;
	slot->seq	= mq->next_seq++;
	slot->prio	= prio;
	slot->len	= (uint32_t)len;
	__ntapi->tt_generic_memcpy(slot->data,msg,len);
	mq->curmsgs++;

	return 0;
}

/* 接收: 0 成功 (*got 返回长度) / 负 errno */
static int __psx_mq_recv_locked(
	struct __psx_mq *	mq,
	unsigned char *		msg,
	size_t			len,
	unsigned *		prio,
	size_t *		got)
{
	int			i, best;
	uint32_t		best_prio, best_seq;
	struct __psx_mq_msg *	slot;

	if (mq->curmsgs == 0)
		return -EAGAIN;

	best		= -1;
	best_prio	= 0;
	best_seq	= 0xFFFFFFFFu;

	for (i = 0; i < PSX_MQ_SLOTS; i++) {
		slot = &mq->msg[i];
		if (!slot->used)
			continue;
		if ((slot->prio > best_prio) ||
			((slot->prio == best_prio) && (slot->seq < best_seq))) {
			best		= i;
			best_prio	= slot->prio;
			best_seq	= slot->seq;
		}
	}

	if (best < 0)
		return -EAGAIN;

	slot = &mq->msg[best];

	/* POSIX: 缓冲小于队列 mq_msgsize → EMSGSIZE */
	if (len < (size_t)mq->msgsize)
		return -EMSGSIZE;

	if (got)
		*got = (size_t)slot->len;
	if (prio)
		*prio = slot->prio;
	__ntapi->tt_generic_memcpy(msg,slot->data,slot->len);
	slot->used	= 0;
	mq->curmsgs--;

	return 0;
}

__psx_api
intptr_t __sys_mq_open(const unsigned char * name, int flags, mode_t mode, void * kattr)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __psx_mq *	mq;
	struct __fd *		fd;
	struct __ofd *		ofd;
	intptr_t		fdidx, ofdidx;

	(void)mode;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!name || !*name)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	mq = __psx_mq_lookup(name);

	if (mq) {
		if ((flags & O_CREAT) && (flags & O_EXCL))
			return __psx_sig_epilog(tlca,-EEXIST,EPSXONLY);
	} else {
		if (!(flags & O_CREAT))
			return __psx_sig_epilog(tlca,-ENOENT,EPSXONLY);

		if (!(mq = __psx_mq_create(name,kattr)))
			return __psx_sig_epilog(tlca,-ENFILE,EPSXONLY);
	}

	if (flags & O_NONBLOCK)
		mq->flags |= O_NONBLOCK;

	if (!(fd = __psx_fd_alloc(ctx,&fdidx))) {
		if (mq->curmsgs == 0 && mq->flags == 0)
			mq->used = 0; /* 失败时若为新建空队列则回收 */
		return __psx_sig_epilog(tlca,-EMFILE,EPSXONLY);
	}

	if (!(ofd = __psx_ofd_alloc(ctx,&ofdidx))) {
		__psx_fd_free(ctx,fd);
		if (mq->curmsgs == 0 && mq->flags == 0)
			mq->used = 0;
		return __psx_sig_epilog(tlca,-EMFILE,EPSXONLY);
	}

	fd->ofdidx	= (int32_t)ofdidx;
	fd->flags	= flags;
	fd->refcnt	= 0;
	at_store_32(&fd->invalid,0);

	ofd->info.fdtype	= PSX_FD_OS_CONFIG;
	ofd->info.vfd		= mq;	/* 私有指针: mq 对象 */

	return __psx_sig_epilog(tlca,fdidx,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_mq_unlink(const unsigned char * name)
{
	struct __psx_tlca *	tlca;
	struct __psx_mq *	mq;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!name || !*name)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (!(mq = __psx_mq_lookup(name)))
		return __psx_sig_epilog(tlca,-ENOENT,EPSXONLY);

	mq->used = 0;	/* 槽回收 (已打开 fd 仍可访问内存) */

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_mq_timedsend(int mqd,
	const unsigned char * msg, size_t len, unsigned prio,
	const struct timespec * at)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __psx_mq *	mq;
	int			r;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (prio > PSX_MQ_PRIO_MAX)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (!(mq = __psx_mq_from_fd(ctx,mqd)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	for (;;) {
		if ((r = __psx_mq_send_locked(mq,msg,len,prio)) != -EAGAIN)
			break;

		if (mq->flags & O_NONBLOCK)
			return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);

		if (at && __psx_mq_deadline_reached(at))
			return __psx_sig_epilog(tlca,-ETIMEDOUT,EPSXONLY);

		__psx_mq_delay(1);
	}

	return __psx_sig_epilog(tlca,r,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_mq_timedreceive(int mqd,
	unsigned char * msg, size_t len, unsigned * prio,
	const struct timespec * at)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __psx_mq *	mq;
	size_t			got;
	int			r;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(mq = __psx_mq_from_fd(ctx,mqd)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	for (;;) {
		got = 0;
		if ((r = __psx_mq_recv_locked(mq,msg,len,prio,&got)) != -EAGAIN)
			break;

		if (mq->flags & O_NONBLOCK)
			return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);

		if (at && __psx_mq_deadline_reached(at))
			return __psx_sig_epilog(tlca,-ETIMEDOUT,EPSXONLY);

		__psx_mq_delay(1);
	}

	if (r)
		return __psx_sig_epilog(tlca,r,NT_STATUS_SUCCESS);

	return __psx_sig_epilog(tlca,(intptr_t)got,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_mq_notify(int mqd, const void * ksev)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __psx_mq *	mq;
	const int *		p;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(mq = __psx_mq_from_fd(ctx,mqd)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	/* sigevent 布局: { union sigval(8); int sigev_signo; int sigev_notify; ... } */
	p = (const int *)ksev;

	if (!ksev) {
		mq->notify_notify	= PSX_MQ_SIGEV_NONE;
		mq->notify_signo	= 0;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	}

	if ((p[2] != PSX_MQ_SIGEV_NONE) && (p[2] != PSX_MQ_SIGEV_SIGNAL) &&
		(p[2] != PSX_MQ_SIGEV_THREAD) && (p[2] != PSX_MQ_SIGEV_THREAD_ID))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	mq->notify_notify	= p[2];
	mq->notify_signo	= p[1];

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_mq_getsetattr(int mqd,
	const void * knew, void * kold)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __psx_mq *	mq;
	long *			pold;
	const long *		pnew;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(mq = __psx_mq_from_fd(ctx,mqd)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	pnew = (const long *)knew;
	pold = (long *)kold;

	/* struct mq_attr { long flags,maxmsg,msgsize,curmsgs,__unused[4] } */
	if (pnew)
		if ((pnew[0] & ~(long)O_NONBLOCK) == 0)
			mq->flags = (uint32_t)pnew[0];

	if (pold) {
		pold[0] = (long)mq->flags;
		pold[1] = (long)mq->maxmsg;
		pold[2] = (long)mq->msgsize;
		pold[3] = (long)mq->curmsgs;
		pold[4] = pold[5] = pold[6] = pold[7] = 0;
	}

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
