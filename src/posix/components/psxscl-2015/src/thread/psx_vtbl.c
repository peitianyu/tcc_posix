/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

/* tcc_posix (R12): musl __unmapself -> __psx_vtbl->unmapself(base,size).
 * musl detached 线程退出路径 (aio worker 等) 在 __pthread_exit 尾部调用
 * __unmapself 释放 mmap 的线程栈并终止线程。musl-nt64 把它转发到本 vtbl。
 *
 * 约束: 释放本线程栈后不能再触碰栈内存 (返回/调用都会因栈页已释放而崩),
 * 因此先把清理工作切到静态共享栈上执行, 再用 __psx_tlca_epilog
 * (jmp 而非 call) 终止线程, 全程不读写已释放的线程栈。 */

#include <ntapi/nt_atomic.h>
#include <psxscl/psxscl.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_impl.h"

/* 供 __psx_tlca_epilog 读取的退出状态 (全局, 非栈内存) */
static intptr_t		__psx_unmap_status;

/* musl mmap 的线程栈区 (base,size), 由 unmapself 传入 */
static void *		__psx_unmap_base;
static size_t		__psx_unmap_size;

/* 共享栈: 释放原线程栈后, 后续清理在此执行 */
static char		__psx_unmap_stack[256 + 16];

static void __psx_unmap_do(void);   /* 运行在共享栈上 */


static void __psx_unmap_do(void)
{
	struct __psx_tlca *	tlca;
	void *			addr;
	size_t			size;

	/* 线程计数递减 (与 __clone_thunk 创建时的 inc 对应) */
	at_locked_xsub(&pthreads, 1);

	/* 释放 TLCA (独立一页, 与线程栈区无关) */
	tlca = __tlca_self();
	addr = tlca;
	size = tlca->tlca_size;
	__ntapi->zw_free_virtual_memory(
		rtdata->hprocess_self,&addr,&size,NT_MEM_RELEASE);

	/* 释放 musl mmap 的线程栈区 */
	addr = __psx_unmap_base;
	size = __psx_unmap_size;
	__ntapi->zw_free_virtual_memory(
		rtdata->hprocess_self,&addr,&size,NT_MEM_RELEASE);

	/* 终止线程 (epilog: jmp ZwTerminateThread, 不读/不写已释放栈) */
	__psx_unmap_status = 0;
	__psx_tlca_epilog(&__psx_unmap_status,
		__ntapi->zw_terminate_thread);

	/* unreachable */
}


static void __psx_unmapself_sys(void * base, void * sz)
{
	char *			top;

	__psx_unmap_base = base;
	__psx_unmap_size = (size_t)sz;

	/* 切到共享栈 (16 对齐), 不再依赖即将释放的线程栈 */
	top = (char *)(((uintptr_t)__psx_unmap_stack + sizeof(__psx_unmap_stack))
		& ~(uintptr_t)15);
	__psx_tlca_prolog(__psx_unmap_do, top);

	/* unreachable */
}


static void __psx_convert_thread_sys(void)
{
	/* todo: 第三方线程转换 (未实现, 当前无调用方) */
}


static void * __psx_get_osfhandle_sys(int fd)
{
	(void)fd;
	return 0;
}


struct __psx_vtbl	__psx_vtbl_impl = {
	__psx_convert_thread_sys,
	__psx_unmapself_sys,
	__psx_get_osfhandle_sys
};


struct __psx_vtbl * __psx_get_psx_vtbl(void)
{
	return &__psx_vtbl_impl;
}
