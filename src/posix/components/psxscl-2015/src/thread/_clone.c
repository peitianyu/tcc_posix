/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  __sys_clone: POSIX thread creation (nt64)           */
/*  kernel32 CreateThread based (ntapi's NtCreateThread */
/*  wrapper is a 10-arg native API but was called with  */
/*  an 8-arg mcontext signature -> garbage thread RIP). */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_impl.h"
#include "psx_tlca.h"
#include "psx_helper.h"
#include "psx_limits.h"
#include "psx.h"

struct __clone_thunk_ctx {
	uintptr_t	entry;
	uintptr_t	arg;
	uintptr_t	tls;
	uintptr_t	ctid;
	void *		ptlca;	/* tcc_posix: 主线程预分配的 worker tlca */
};

static struct __clone_thunk_ctx	__clone_ctxs[64];
static volatile int			__clone_ctx_busy[64];

typedef void * (__stdcall *__clone_create_thread_fn)(
	void *, size_t, void *(__stdcall *)(void *), void *, unsigned, unsigned *);
typedef int (__stdcall *__clone_close_handle_fn)(void *);
static __clone_create_thread_fn	__clone_pCreateThread;
static __clone_close_handle_fn	__clone_pCloseHandle;


/* minimal psxscl tlca for the new thread (avoids full __psx_tlca_init) */
static int32_t __clone_tlca_init(void)
{
	struct __psx_tlca *	tlca;
	size_t			tlca_size;
	int32_t			status;
	nt_tib *		tib;

	tlca_size = __PSX_PAGE_SIZE;
	tlca = 0;

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&tlca,0,&tlca_size,
			NT_MEM_COMMIT,NT_PAGE_READWRITE)))
		return status;

	__ntapi->tt_aligned_block_memset(tlca,0,tlca_size);

	tlca->tlca_addr  = tlca;
	tlca->tlca_size  = tlca_size;
	tlca->buflen     = tlca_size - (size_t)&((struct __psx_tlca *)0)->buffer;
	tlca->cfalert    = NT_SYNC_ALERTABLE;
	tlca->cfnonalert = NT_SYNC_NON_ALERTABLE;
	tlca->cfzerowait = &tlca->zerowait;
	tlca->cfinfinity = 0;
	tlca->ctx        = &rtctx;

	tib = pe_get_teb_address();
	tlca->tib_system.stack_base  = tib->stack_base;
	tlca->tib_system.stack_limit = tib->stack_limit;

	*(__tls_slot_addr()) = tlca;
	return 0;
}

static void __clone_set_teb_libc_slot(uintptr_t val)
{
	void *		tib;
	uintptr_t	idx = wintls_libc_idx;

	tib = pe_get_teb_address();
	if (idx < 64)
		((uintptr_t **)((uintptr_t)tib + 0x1480))[idx] = (uintptr_t *)val;
	else {
		uintptr_t **	xslots;

		xslots = *(uintptr_t ***)((uintptr_t)tib + 0x1780);
		xslots[idx - 64] = (uintptr_t *)val;
	}
}

volatile int __tcc_thunk_enter = 0;

static void * __stdcall __clone_thunk(void * p)
{
	__tcc_thunk_enter++;
	struct __clone_thunk_ctx * c = (struct __clone_thunk_ctx *)p;
	int i = (int)(c - __clone_ctxs);
	int32_t status;

	/* psxscl per-thread context (TEB sys slot; __tlca_self needs it)
	   主线程预分配的 tlca: worker 线程自身调用
	   zw_allocate_virtual_memory 会卡 (TLS 未初始化) */
	if (c->ptlca) {
		struct __psx_tlca *tlca = (struct __psx_tlca *)c->ptlca;
		nt_tib *tib;
		tlca->tlca_addr  = tlca;
		tlca->tlca_size  = __PSX_PAGE_SIZE;
		tlca->buflen     = __PSX_PAGE_SIZE - (size_t)&((struct __psx_tlca *)0)->buffer;
		tlca->cfalert    = NT_SYNC_ALERTABLE;
		tlca->cfnonalert = NT_SYNC_NON_ALERTABLE;
		tlca->cfzerowait = &tlca->zerowait;
		tlca->cfinfinity = 0;
		tlca->ctx        = &rtctx;
		tib = pe_get_teb_address();
		tlca->tib_system.stack_base  = tib->stack_base;
		tlca->tib_system.stack_limit = tib->stack_limit;
		*(__tls_slot_addr()) = tlca;
	}

	/* musl __pthread_self: *sys_slot -> tlca->pthread_self */
	((struct __psx_tlca *)*__tls_slot_addr())->pthread_self = (void *)c->tls;

	/* tcc_posix: 计数 pthreads (原版 __psx_tlca_init 会 inc, 但我们
	   绕过它用主线程预分配 tlca; 不补计数则 daemon 在 worker 退出时
	   把 pthreads 减到 0 误判为"最后一个线程" → ZwTerminateProcess
	   杀进程, main 的 join 永远等不到) */
	at_locked_inc(&pthreads);

	/* pthread self -> TEB libc slot */
	__clone_set_teb_libc_slot(c->tls);

	/* ctid: cleared by __psx_exit on thread exit (join) */
	__sys_set_tid_address((int *)c->ctid);

	/* entry(arg): musl start() never returns (SYS_exit loop) */
	status = ((int32_t (*)(void *))c->entry)((void *)c->arg);

	__clone_ctx_busy[i] = 0;
	__ntapi->zw_terminate_thread(
		NT_CURRENT_THREAD_HANDLE,
		status);

	return 0; /* unreachable */
}

__psx_api
long __sys_clone(
	uintptr_t	flags,
	void *		child_stack,
	void *		ptid,
	void *		ctid,
	struct pt_regs *regs)
{
	struct __clone_thunk_ctx *	ctx;
	void *				hthread;
	void *				kernel32;
	unsigned			tid;
	int i;

	(void)flags;
	(void)child_stack;

	if (!__clone_pCreateThread) {
		kernel32 = pe_get_kernel32_module_handle();
		__clone_pCreateThread = (__clone_create_thread_fn)
			pe_get_procedure_address(kernel32,"CreateThread");
		__clone_pCloseHandle = (__clone_close_handle_fn)
			pe_get_procedure_address(kernel32,"CloseHandle");

	}

	for (i = 0; i < 64; i++) {
		if (__clone_ctx_busy[i] == 0) {
			__clone_ctx_busy[i] = 1;
			break;
		}
	}
	if (i == 64)
		return -EAGAIN;

	ctx = &__clone_ctxs[i];
	ctx->entry = regs->rip;
	ctx->arg   = regs->rcx;
	ctx->tls   = regs->rdx;
	ctx->ctid  = (uintptr_t)ctid;

	/* tcc_posix: 主线程预分配 worker tlca (worker 线程自身调用
	   zw_allocate_virtual_memory 会卡, 因 TLS 未初始化) */
	ctx->ptlca = 0;
	{
		void *tmp = 0;
		size_t sz = __PSX_PAGE_SIZE;
		if (!__ntapi->zw_allocate_virtual_memory(
				NT_CURRENT_PROCESS_HANDLE, &tmp, 0, &sz,
				NT_MEM_COMMIT, NT_PAGE_READWRITE)) {
			unsigned char *cp = (unsigned char *)tmp;
			size_t n = sz;
			while (n--) *cp++ = 0;
			ctx->ptlca = tmp;
		}
	}

	/* tcc_posix: TCC 的 6 参函数指针调用不遵循 Win64 shadow space
	   约定, 用汇编转换 (源自 cosmopolitan __sysv2nt) */
	{
		extern void *psx_sysv2nt6(void *fn, unsigned long long a1,
			unsigned long long a2, unsigned long long a3,
			unsigned long long a4, unsigned long long a5,
			unsigned long long a6);
		hthread = psx_sysv2nt6(
			(void *)__clone_pCreateThread,
			(unsigned long long)0,          /* lpThreadAttributes */
			(unsigned long long)0,          /* dwStackSize */
			(unsigned long long)(uintptr_t)__clone_thunk,
			(unsigned long long)(uintptr_t)ctx,
			(unsigned long long)0,          /* flags */
			(unsigned long long)(uintptr_t)&tid);
	}
	if (!hthread) {
		__clone_ctx_busy[i] = 0;
		return -EAGAIN;
	}

	/* tcc_posix: 新线程创建后主动让出, 否则 Windows 调度器可能
	   让 worker 饿死 (musl 链里 CreateThread 后不主动让出时,
	   worker 长时间不被调度; SwitchToThread 密集让出可 100% 启动) */
	{
		typedef int (__stdcall *swt_fn)(void);
		static swt_fn pSwitchToThread;
		int k;
		if (!pSwitchToThread) {
			void *k32 = pe_get_kernel32_module_handle();
			pSwitchToThread = (swt_fn)pe_get_procedure_address(
				k32, "SwitchToThread");
		}
		for (k = 0; k < 20000 && pSwitchToThread; k++) {
			pSwitchToThread();
			if (__tcc_thunk_enter) break;
		}
	}

	__clone_pCloseHandle(hthread);

	/* CLONE_PARENT_SETTID: parent writes non-zero tid (musl join waits) */
	if (ptid)
		*(int *)ptid = (int)tid;

	return 0;
}
