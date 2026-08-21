/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/
/* tcc_posix (R10b): 补全 POSIX System V IPC (msg/sem/shm) syscall.
 *
 * musl 库层 (src/ipc/*.c) 在, 但 SYS_msgget(68)/msgsnd(69)/msgrcv(70)/
 * msgctl(71), SYS_semget(64)/semop(65)/semctl(66)/semtimedop(220),
 * SYS_shmget(29)/shmat(30)/shmctl(31)/shmdt(67) 未注册 → 原生 syscall
 * 一律 ENOSYS。此文件在 PSX 接口层补这些 syscall, musl 原版不动。
 *
 * 设计取舍 (接口层, 静态槽表, 不递归 musl malloc):
 * - msg/sem/shm 各一张进程内静态槽表 PSX_IPC_CAP 个; id 采用
 *   (seq<<16 | idx+1) 编码, 避免槽复用后旧 id 误指新对象。
 * - shmat 直接返回槽内静态缓冲地址 (单进程语义), nattch 计数;
 *   shmdt 按地址区间匹配槽。
 * - semop/semtimedop 阻塞语义: 操作不可立即执行时轮询 + 短延时
 *   (与 _mq.c 同款), IPC_NOWAIT → EAGAIN; semtimedop 超时 → EAGAIN。
 * - msgrcv 支持 msgtyp==0/正/负 与 MSG_NOERROR 截断 / MSG_EXCEPT。
 * - 已知局限 (记录待用户决策): 进程内实现, 无跨进程 IPC; SEM_UNDO
 *   忽略; SHM_LOCK/UNLOCK 空操作; sem 阻塞不做信号中断。
 */

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_tlca.h"
#include "psx_impl.h"
#include "psx.h"

#define PSX_IPC_CAP		64

/* id 编码: (seq<<16)|(idx+1) */
#define PSX_IPC_SEQ_MASK	0xFFFF

/* sys/ipc.h 常量 (IPC_64=0, 故 cmd 无需剥离) */
#define PSX_IPC_CREAT		01000
#define PSX_IPC_EXCL		02000
#define PSX_IPC_NOWAIT		04000
#define PSX_IPC_PRIVATE		((key_t)0)
#define PSX_IPC_RMID		0
#define PSX_IPC_SET		1
#define PSX_IPC_STAT		2

/* E2BIG 不在 psx_errno.h 中 */
#define PSX_E2BIG		7

/* sys/msg.h */
#define PSX_MSG_NOERROR		010000
#define PSX_MSG_EXCEPT		020000
#define PSX_MSG_SLOTS		32
#define PSX_MSG_MSGSZ		2048
#define PSX_MSG_QBYTES		16384

/* sys/sem.h */
#define PSX_SEM_NSEMS		32
#define PSX_SEM_UNDO		0x1000
#define PSX_SEM_GETPID		11
#define PSX_SEM_GETVAL		12
#define PSX_SEM_GETALL		13
#define PSX_SEM_GETNCNT		14
#define PSX_SEM_GETZCNT		15
#define PSX_SEM_SETVAL		16
#define PSX_SEM_SETALL		17

/* sys/shm.h */
#define PSX_SHM_SIZE		65536
#define PSX_SHM_RDONLY		010000
#define PSX_SHM_RND		020000
#define PSX_SHM_LOCK		11
#define PSX_SHM_UNLOCK		12

/* 与 musl arch/nt64/bits/ipc.h struct ipc_perm 布局一致 (40 字节) */
struct __psx_ipc_perm {
	key_t		key;	/* 4 */
	uid_t		uid;	/* 4 */
	gid_t		gid;	/* 4 */
	uid_t		cuid;	/* 4 */
	gid_t		cgid;	/* 4 */
	mode_t		mode;	/* 4 */
	int32_t		seq;	/* 4 */
	int64_t		pad1;	/* 8 */
	int64_t		pad2;	/* 8 */
};

/* struct msqid_ds (musl bits/msg.h) */
struct __psx_msqid_ds {
	struct __psx_ipc_perm	msg_perm;
	int64_t			msg_stime;
	int64_t			msg_rtime;
	int64_t			msg_ctime;
	unsigned long		msg_cbytes;
	unsigned long		msg_qnum;
	unsigned long		msg_qbytes;
	int32_t			msg_lspid;
	int32_t			msg_lrpid;
	unsigned long		__unused[2];
};

/* struct semid_ds (musl bits/sem.h) */
struct __psx_semid_ds {
	struct __psx_ipc_perm	sem_perm;
	int64_t			sem_otime;
	int64_t			__unused1;
	int64_t			sem_ctime;
	int64_t			__unused2;
	unsigned short		sem_nsems;
	char			__sem_nsems_pad[6];
	int64_t			__unused3;
	int64_t			__unused4;
};

/* struct shmid_ds (musl bits/shm.h) */
struct __psx_shmid_ds {
	struct __psx_ipc_perm	shm_perm;
	size_t			shm_segsz;
	int64_t			shm_atime;
	int64_t			shm_dtime;
	int64_t			shm_ctime;
	int32_t			shm_cpid;
	int32_t			shm_lpid;
	unsigned long		shm_nattch;
	unsigned long		__pad1;
	unsigned long		__pad2;
};

struct __psx_msg {
	uint32_t		used;
	uint32_t		len;
	int64_t			mtype;
	unsigned char		data[PSX_MSG_MSGSZ];
};

struct __psx_msq {
	uint32_t		used;
	uint32_t		seq;
	key_t			key;
	struct __psx_msqid_ds	ds;
	struct __psx_msg	msg[PSX_MSG_SLOTS];
};

struct __psx_sembuf {	/* musl struct sembuf */
	unsigned short		sem_num;
	short			sem_op;
	short			sem_flg;
};

struct __psx_sem {
	uint32_t		used;
	uint32_t		seq;
	key_t			key;
	struct __psx_semid_ds	ds;
	unsigned short		val[PSX_SEM_NSEMS];
	int32_t			pid[PSX_SEM_NSEMS];
};

struct __psx_shm {
	uint32_t		used;
	uint32_t		seq;
	key_t			key;
	struct __psx_shmid_ds	ds;
	unsigned char		data[PSX_SHM_SIZE];
};

static struct __psx_msq __psx_msqs[PSX_IPC_CAP];
static struct __psx_sem __psx_sems[PSX_IPC_CAP];
static struct __psx_shm __psx_shms[PSX_IPC_CAP];
static uint32_t __psx_ipc_seq;

static time_t __psx_ipc_time(void)
{
	struct timespec ts;

	__sys_clock_gettime(0,&ts);

	return (time_t)ts.tv_sec;
}

static pid_t __psx_ipc_pid(void)
{
	return (pid_t)__sys_getpid();
}

static void __psx_ipc_perm_init(
	struct __psx_ipc_perm *	perm,
	key_t			key,
	mode_t			mode)
{
	perm->key	= key;
	perm->uid	= __psx.__uid;
	perm->gid	= __psx.__gid;
	perm->cuid	= __psx.__uid;
	perm->cgid	= __psx.__gid;
	perm->mode	= mode;
	perm->seq	= (int32_t)__psx_ipc_seq;
	perm->pad1	= 0;
	perm->pad2	= 0;
}

/* id: (seq<<16)|(idx+1); seq==0 时低 16 位不可为 0 (idx+1>=1) */
static intptr_t __psx_ipc_id(int idx, uint32_t seq)
{
	return (intptr_t)(((seq & PSX_IPC_SEQ_MASK) << 16) |
		((uint32_t)idx + 1));
}

static int __psx_ipc_idx(intptr_t id, uint32_t seq)
{
	uint32_t lo, hi;

	lo = (uint32_t)id & PSX_IPC_SEQ_MASK;
	hi = ((uint32_t)id >> 16) & PSX_IPC_SEQ_MASK;

	if ((lo == 0) || (lo > PSX_IPC_CAP))
		return -1;
	if (hi != (seq & PSX_IPC_SEQ_MASK))
		return -1;

	return (int)(lo - 1);
}

static void __psx_ipc_delay(void)
{
	nt_large_integer delay;

	delay.quad = 10000; /* 1ms */
	__ntapi->zw_delay_execution(0,&delay);
}

static int __psx_ipc_deadline(const struct timespec * ts)
{
	struct timespec now;

	__sys_clock_gettime(0,&now);

	return (now.tv_sec > ts->tv_sec) ||
		((now.tv_sec == ts->tv_sec) && (now.tv_nsec >= ts->tv_nsec));
}

/********************************************************/
/* msg (消息队列)                                       */
/********************************************************/

static struct __psx_msq * __psx_msq_lookup(key_t key)
{
	int i;

	for (i = 0; i < PSX_IPC_CAP; i++)
		if (__psx_msqs[i].used && (__psx_msqs[i].key == key))
			return &__psx_msqs[i];

	return 0;
}

static struct __psx_msq * __psx_msq_alloc(key_t key, mode_t mode)
{
	int i;

	for (i = 0; i < PSX_IPC_CAP; i++)
		if (!__psx_msqs[i].used)
			break;

	if (i >= PSX_IPC_CAP)
		return 0;

	__psx_ipc_seq++;
	__ntapi->tt_aligned_block_memset(&__psx_msqs[i],0,sizeof(__psx_msqs[i]));
	__psx_msqs[i].used	= 1;
	__psx_msqs[i].seq	= __psx_ipc_seq;
	__psx_msqs[i].key	= key;
	__psx_ipc_perm_init(&__psx_msqs[i].ds.msg_perm,key,mode);
	__psx_msqs[i].ds.msg_qbytes	= PSX_MSG_QBYTES;
	__psx_msqs[i].ds.msg_ctime	= __psx_ipc_time();

	return &__psx_msqs[i];
}

static struct __psx_msq * __psx_msq_from_id(intptr_t id)
{
	int idx;

	for (idx = 0; idx < PSX_IPC_CAP; idx++)
		if (__psx_ipc_idx(id,__psx_msqs[idx].seq) == idx)
			return __psx_msqs[idx].used ? &__psx_msqs[idx] : 0;

	return 0;
}

__psx_api
intptr_t __sys_msgget(key_t key, int flag)
{
	struct __psx_tlca *	tlca;
	struct __psx_msq *	q;
	mode_t			mode;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	mode = (mode_t)(flag & 0777);

	if (key != PSX_IPC_PRIVATE) {
		if ((q = __psx_msq_lookup(key))) {
			if ((flag & PSX_IPC_CREAT) && (flag & PSX_IPC_EXCL))
				return __psx_sig_epilog(tlca,-EEXIST,EPSXONLY);
			return __psx_sig_epilog(tlca,
				__psx_ipc_id((int)(q - __psx_msqs),q->seq),
				NT_STATUS_SUCCESS);
		}
	}

	if (!(flag & PSX_IPC_CREAT))
		return __psx_sig_epilog(tlca,-ENOENT,EPSXONLY);

	if (!(q = __psx_msq_alloc(key,mode)))
		return __psx_sig_epilog(tlca,-ENOSPC,EPSXONLY);

	return __psx_sig_epilog(tlca,
		__psx_ipc_id((int)(q - __psx_msqs),q->seq),
		NT_STATUS_SUCCESS);
}

/* 返回第一个可发送的空槽 (满 → 0) */
static struct __psx_msg * __psx_msq_slot_free(struct __psx_msq * q)
{
	int i;

	for (i = 0; i < PSX_MSG_SLOTS; i++)
		if (!q->msg[i].used)
			return &q->msg[i];

	return 0;
}

/* 按 msgtyp 选消息: 0→队首; >0→同 type; <0→最低 type 且 <= -msgtyp */
static struct __psx_msg * __psx_msq_pick(
	struct __psx_msq *	q,
	long			msgtyp,
	int			flags)
{
	int			i;
	struct __psx_msg *	best;

	if (msgtyp == 0) {
		for (i = 0; i < PSX_MSG_SLOTS; i++)
			if (q->msg[i].used)
				return &q->msg[i];
		return 0;
	}

	if (msgtyp > 0) {
		for (i = 0; i < PSX_MSG_SLOTS; i++) {
			if (!q->msg[i].used)
				continue;
			if (flags & PSX_MSG_EXCEPT) {
				if (q->msg[i].mtype != msgtyp)
					return &q->msg[i];
			} else if (q->msg[i].mtype == msgtyp) {
				return &q->msg[i];
			}
		}
		return 0;
	}

	/* msgtyp < 0: 最小 mtype 且 <= -msgtyp */
	best = 0;
	for (i = 0; i < PSX_MSG_SLOTS; i++) {
		if (!q->msg[i].used)
			continue;
		if (q->msg[i].mtype > (int64_t)(-msgtyp))
			continue;
		if (!best || (q->msg[i].mtype < best->mtype))
			best = &q->msg[i];
	}

	return best;
}

__psx_api
intptr_t __sys_msgsnd(int qid, const void * msg, size_t len, int flag)
{
	struct __psx_tlca *	tlca;
	struct __psx_msq *	q;
	struct __psx_msg *	slot;
	const long *		mp;
	long			mtype;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(q = __psx_msq_from_id(qid)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	if (!msg)
		return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);

	mp	= (const long *)msg;
	mtype	= (long)mp[0];
	if (mtype <= 0)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	if (len > PSX_MSG_MSGSZ)
		return __psx_sig_epilog(tlca,-PSX_E2BIG,EPSXONLY);

	for (;;) {
		if ((slot = __psx_msq_slot_free(q))) {
			slot->used	= 1;
			slot->len	= (uint32_t)len;
			slot->mtype	= mtype;
			__ntapi->tt_generic_memcpy(slot->data,
				(const unsigned char *)msg + sizeof(long),len);
			q->ds.msg_qnum++;
			q->ds.msg_cbytes += (unsigned long)len;
			q->ds.msg_lspid = __psx_ipc_pid();
			q->ds.msg_stime = __psx_ipc_time();
			return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
		}

		if (flag & PSX_IPC_NOWAIT)
			return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);

		__psx_ipc_delay();
	}
}

__psx_api
intptr_t __sys_msgrcv(int qid, void * msg, size_t len, long msgtyp, int flag)
{
	struct __psx_tlca *	tlca;
	struct __psx_msq *	q;
	struct __psx_msg *	slot;
	long *			mp;
	size_t			got;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(q = __psx_msq_from_id(qid)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	if (!msg)
		return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);

	for (;;) {
		if ((slot = __psx_msq_pick(q,msgtyp,flag))) {
			got = (size_t)slot->len;
			if (got > len) {
				if (!(flag & PSX_MSG_NOERROR))
					return __psx_sig_epilog(tlca,-PSX_E2BIG,EPSXONLY);
				got = len;	/* 截断 */
			}
			mp	= (long *)msg;
			mp[0]	= (long)slot->mtype;
			__ntapi->tt_generic_memcpy(
				(unsigned char *)msg + sizeof(long),
				slot->data,got);
			slot->used	= 0;
			q->ds.msg_qnum--;
			q->ds.msg_cbytes -= (unsigned long)slot->len;
			q->ds.msg_lrpid = __psx_ipc_pid();
			q->ds.msg_rtime = __psx_ipc_time();
			return __psx_sig_epilog(tlca,(intptr_t)got,NT_STATUS_SUCCESS);
		}

		if (flag & PSX_IPC_NOWAIT)
			return __psx_sig_epilog(tlca,-ENOMSG,EPSXONLY);

		__psx_ipc_delay();
	}
}

__psx_api
intptr_t __sys_msgctl(int qid, int cmd, struct __psx_msqid_ds * buf)
{
	struct __psx_tlca *	tlca;
	struct __psx_msq *	q;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(q = __psx_msq_from_id(qid)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	switch (cmd) {
	case PSX_IPC_RMID:
		q->used = 0;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_IPC_STAT:
		if (!buf)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		*buf = q->ds;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_IPC_SET:
		if (!buf)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		q->ds.msg_perm.uid	= buf->msg_perm.uid;
		q->ds.msg_perm.gid	= buf->msg_perm.gid;
		q->ds.msg_perm.mode	= buf->msg_perm.mode;
		q->ds.msg_qbytes	= buf->msg_qbytes;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	default:
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	}
}

/********************************************************/
/* sem (信号量)                                         */
/********************************************************/

static struct __psx_sem * __psx_sem_lookup(key_t key)
{
	int i;

	for (i = 0; i < PSX_IPC_CAP; i++)
		if (__psx_sems[i].used && (__psx_sems[i].key == key))
			return &__psx_sems[i];

	return 0;
}

static struct __psx_sem * __psx_sem_alloc(key_t key, int nsems, mode_t mode)
{
	int i;

	for (i = 0; i < PSX_IPC_CAP; i++)
		if (!__psx_sems[i].used)
			break;

	if (i >= PSX_IPC_CAP)
		return 0;

	__psx_ipc_seq++;
	__ntapi->tt_aligned_block_memset(&__psx_sems[i],0,sizeof(__psx_sems[i]));
	__psx_sems[i].used	= 1;
	__psx_sems[i].seq	= __psx_ipc_seq;
	__psx_sems[i].key	= key;
	__psx_sems[i].ds.sem_nsems = (unsigned short)nsems;
	__psx_ipc_perm_init(&__psx_sems[i].ds.sem_perm,key,mode);
	__psx_sems[i].ds.sem_ctime = __psx_ipc_time();

	return &__psx_sems[i];
}

static struct __psx_sem * __psx_sem_from_id(intptr_t id)
{
	int idx;

	for (idx = 0; idx < PSX_IPC_CAP; idx++)
		if (__psx_ipc_idx(id,__psx_sems[idx].seq) == idx)
			return __psx_sems[idx].used ? &__psx_sems[idx] : 0;

	return 0;
}

__psx_api
intptr_t __sys_semget(key_t key, int nsems, int fl)
{
	struct __psx_tlca *	tlca;
	struct __psx_sem *	sem;
	mode_t			mode;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((nsems <= 0) || (nsems > PSX_SEM_NSEMS))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	mode = (mode_t)(fl & 0777);

	if (key != PSX_IPC_PRIVATE) {
		if ((sem = __psx_sem_lookup(key))) {
			if ((fl & PSX_IPC_CREAT) && (fl & PSX_IPC_EXCL))
				return __psx_sig_epilog(tlca,-EEXIST,EPSXONLY);
			if (nsems > (int)sem->ds.sem_nsems)
				return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
			return __psx_sig_epilog(tlca,
				__psx_ipc_id((int)(sem - __psx_sems),sem->seq),
				NT_STATUS_SUCCESS);
		}
	}

	if (!(fl & PSX_IPC_CREAT))
		return __psx_sig_epilog(tlca,-ENOENT,EPSXONLY);

	if (!(sem = __psx_sem_alloc(key,nsems,mode)))
		return __psx_sig_epilog(tlca,-ENOSPC,EPSXONLY);

	return __psx_sig_epilog(tlca,
		__psx_ipc_id((int)(sem - __psx_sems),sem->seq),
		NT_STATUS_SUCCESS);
}

/* 原子尝试应用操作集: 1=已应用, 0=阻塞, 负=错误 */
static int __psx_sem_try(
	struct __psx_sem *		sem,
	const struct __psx_sembuf *	ops,
	size_t				n)
{
	size_t			i;
	unsigned short		num;
	short			op;

	for (i = 0; i < n; i++) {
		num = ops[i].sem_num;
		op  = ops[i].sem_op;

		if (num >= sem->ds.sem_nsems)
			return -EFBIG;
		if ((op < 0) && ((int)sem->val[num] < (int)(-op)))
			return 0;
		if ((op == 0) && (sem->val[num] != 0))
			return 0;
	}

	for (i = 0; i < n; i++) {
		num = ops[i].sem_num;
		op  = ops[i].sem_op;
		sem->val[num] = (unsigned short)((int)sem->val[num] + (int)op);
		sem->pid[num] = __psx_ipc_pid();
	}
	sem->ds.sem_otime = __psx_ipc_time();

	return 1;
}

static intptr_t __psx_sem_op(
	struct __psx_tlca *	tlca,
	int			id,
	const void *		buf,
	size_t			n,
	const struct timespec *	ts)
{
	struct __psx_sem *	sem;
	const struct __psx_sembuf *ops;
	int			r;
	size_t			i;

	ops = (const struct __psx_sembuf *)buf;

	if (!(sem = __psx_sem_from_id(id)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	/* 任一操作带 IPC_NOWAIT → 不阻塞 */
	for (i = 0; i < n; i++)
		if (ops[i].sem_flg & PSX_IPC_NOWAIT)
			break;

	for (;;) {
		r = __psx_sem_try(sem,ops,n);

		if (r > 0)	/* 成功应用 */
			return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
		if (r < 0)	/* -EFBIG 等 */
			return __psx_sig_epilog(tlca,r,NT_STATUS_SUCCESS);

		if (i < n)	/* 有 IPC_NOWAIT */
			return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);

		if (ts && __psx_ipc_deadline(ts))
			return __psx_sig_epilog(tlca,-EAGAIN,EPSXONLY);

		__psx_ipc_delay();
	}
}

__psx_api
intptr_t __sys_semop(int id, const struct __psx_sembuf * buf, size_t n)
{
	struct __psx_tlca *	tlca;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	return __psx_sem_op(tlca,id,buf,n,0);
}

__psx_api
intptr_t __sys_semtimedop(int id, const struct __psx_sembuf * buf, size_t n, const struct timespec * ts)
{
	struct __psx_tlca *	tlca;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	return __psx_sem_op(tlca,id,buf,n,ts);
}

__psx_api
intptr_t __sys_semctl(int id, int num, int cmd, void * arg)
{
	struct __psx_tlca *	tlca;
	struct __psx_sem *	sem;
	unsigned short *	array;
	int			i, val;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(sem = __psx_sem_from_id(id)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	switch (cmd) {
	case PSX_IPC_RMID:
		sem->used = 0;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_IPC_STAT:
		if (!arg)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		*(struct __psx_semid_ds *)arg = sem->ds;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_IPC_SET:
		if (!arg)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		sem->ds.sem_perm.uid	= ((struct __psx_semid_ds *)arg)->sem_perm.uid;
		sem->ds.sem_perm.gid	= ((struct __psx_semid_ds *)arg)->sem_perm.gid;
		sem->ds.sem_perm.mode	= ((struct __psx_semid_ds *)arg)->sem_perm.mode;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_SEM_GETVAL:
		if (((unsigned)num) >= sem->ds.sem_nsems)
			return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
		return __psx_sig_epilog(tlca,(intptr_t)sem->val[num],NT_STATUS_SUCCESS);
	case PSX_SEM_GETPID:
		if (((unsigned)num) >= sem->ds.sem_nsems)
			return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
		return __psx_sig_epilog(tlca,(intptr_t)sem->pid[num],NT_STATUS_SUCCESS);
	case PSX_SEM_GETNCNT:
	case PSX_SEM_GETZCNT:
		if (((unsigned)num) >= sem->ds.sem_nsems)
			return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_SEM_SETVAL:
		if (((unsigned)num) >= sem->ds.sem_nsems)
			return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
		val = (int)(intptr_t)arg;	/* musl 把 int 塞进 union, 低 32 位 */
		sem->val[num] = (unsigned short)val;
		sem->pid[num] = __psx_ipc_pid();
		sem->ds.sem_ctime = __psx_ipc_time();
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_SEM_GETALL:
		if (!arg)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		array = (unsigned short *)arg;
		for (i = 0; i < (int)sem->ds.sem_nsems; i++)
			array[i] = sem->val[i];
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_SEM_SETALL:
		if (!arg)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		array = (unsigned short *)arg;
		for (i = 0; i < (int)sem->ds.sem_nsems; i++) {
			sem->val[i] = array[i];
			sem->pid[i] = __psx_ipc_pid();
		}
		sem->ds.sem_ctime = __psx_ipc_time();
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	default:
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	}
}

/********************************************************/
/* shm (共享内存)                                       */
/********************************************************/

static struct __psx_shm * __psx_shm_lookup(key_t key)
{
	int i;

	for (i = 0; i < PSX_IPC_CAP; i++)
		if (__psx_shms[i].used && (__psx_shms[i].key == key))
			return &__psx_shms[i];

	return 0;
}

static struct __psx_shm * __psx_shm_alloc(key_t key, size_t size, mode_t mode)
{
	int i;

	for (i = 0; i < PSX_IPC_CAP; i++)
		if (!__psx_shms[i].used)
			break;

	if (i >= PSX_IPC_CAP)
		return 0;

	__psx_ipc_seq++;
	__ntapi->tt_aligned_block_memset(&__psx_shms[i],0,sizeof(__psx_shms[i]));
	__psx_shms[i].used	= 1;
	__psx_shms[i].seq	= __psx_ipc_seq;
	__psx_shms[i].key	= key;
	__psx_shms[i].ds.shm_segsz = size;
	__psx_shms[i].ds.shm_cpid = __psx_ipc_pid();
	__psx_ipc_perm_init(&__psx_shms[i].ds.shm_perm,key,mode);
	__psx_shms[i].ds.shm_ctime = __psx_ipc_time();

	return &__psx_shms[i];
}

static struct __psx_shm * __psx_shm_from_id(intptr_t id)
{
	int idx;

	for (idx = 0; idx < PSX_IPC_CAP; idx++)
		if (__psx_ipc_idx(id,__psx_shms[idx].seq) == idx)
			return __psx_shms[idx].used ? &__psx_shms[idx] : 0;

	return 0;
}

__psx_api
intptr_t __sys_shmget(key_t key, size_t size, int flag)
{
	struct __psx_tlca *	tlca;
	struct __psx_shm *	shm;
	mode_t			mode;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	mode = (mode_t)(flag & 0777);

	if (key != PSX_IPC_PRIVATE) {
		if ((shm = __psx_shm_lookup(key))) {
			if ((flag & PSX_IPC_CREAT) && (flag & PSX_IPC_EXCL))
				return __psx_sig_epilog(tlca,-EEXIST,EPSXONLY);
			if (size > shm->ds.shm_segsz)
				return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
			return __psx_sig_epilog(tlca,
				__psx_ipc_id((int)(shm - __psx_shms),shm->seq),
				NT_STATUS_SUCCESS);
		}
	}

	if (!(flag & PSX_IPC_CREAT))
		return __psx_sig_epilog(tlca,-ENOENT,EPSXONLY);

	if (size > PSX_SHM_SIZE)
		return __psx_sig_epilog(tlca,-ENOMEM,EPSXONLY);

	if (!(shm = __psx_shm_alloc(key,size,mode)))
		return __psx_sig_epilog(tlca,-ENOSPC,EPSXONLY);

	return __psx_sig_epilog(tlca,
		__psx_ipc_id((int)(shm - __psx_shms),shm->seq),
		NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_shmat(int id, const void * addr, int flag)
{
	struct __psx_tlca *	tlca;
	struct __psx_shm *	shm;

	(void)addr;
	(void)flag;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(shm = __psx_shm_from_id(id)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	shm->ds.shm_nattch++;
	shm->ds.shm_lpid = __psx_ipc_pid();
	shm->ds.shm_atime = __psx_ipc_time();

	return __psx_sig_epilog(tlca,(intptr_t)&shm->data[0],NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_shmdt(const void * addr)
{
	struct __psx_tlca *	tlca;
	struct __psx_shm *	shm;
	uintptr_t		a;
	int			i;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	a = (uintptr_t)addr;

	for (i = 0; i < PSX_IPC_CAP; i++) {
		shm = &__psx_shms[i];
		if (!shm->used)
			continue;
		if ((a >= (uintptr_t)&shm->data[0]) &&
			(a <  (uintptr_t)&shm->data[0] + shm->ds.shm_segsz)) {
			if (shm->ds.shm_nattch > 0)
				shm->ds.shm_nattch--;
			shm->ds.shm_dtime = __psx_ipc_time();
			return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
		}
	}

	return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
}

__psx_api
intptr_t __sys_shmctl(int id, int cmd, struct __psx_shmid_ds * buf)
{
	struct __psx_tlca *	tlca;
	struct __psx_shm *	shm;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(shm = __psx_shm_from_id(id)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	switch (cmd) {
	case PSX_IPC_RMID:
		shm->used = 0;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_IPC_STAT:
	case 13 /* SHM_STAT */:
		if (!buf)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		*buf = shm->ds;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_IPC_SET:
		if (!buf)
			return __psx_sig_epilog(tlca,-EFAULT,EPSXONLY);
		shm->ds.shm_perm.uid	= buf->shm_perm.uid;
		shm->ds.shm_perm.gid	= buf->shm_perm.gid;
		shm->ds.shm_perm.mode	= buf->shm_perm.mode;
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	case PSX_SHM_LOCK:
	case PSX_SHM_UNLOCK:
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
	default:
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	}
}
