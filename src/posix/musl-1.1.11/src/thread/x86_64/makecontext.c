/* makecontext(): 在给定的独立栈上布置一个入口 fn 及其寄存器参数,
 * 使后续 setcontext/swapcontext 恢复该上下文时直接跳到 fn 执行。
 *
 * linux x86_64 / SysV 版 (nt64 对应版见 musl-nt64/src/thread/nt64/makecontext.c)。
 * 值语义 (与同目录 ucontext.s 约定一致):
 *  - setcontext/swapcontext 恢复时 rsp = 存值+8; 故存值 uc_rsp 必须 %16==0,
 *    使 fn 入口 rsp%16==8 (SysV: 等效 call 压 8B 返回地址后的对齐)。
 *  - 返回地址 __uc_finish 预放于 [存值+8], fn 正常 ret 时兜底自旋。
 *  - uc_mcontext.gregs[REG_RIP] = fn; 其余 GPR 槽按 SysV 传参约定由 setcontext 恢复。
 *  - 参数: makecontext(ucp, fn, argc, a1..an): 前 ≤6 个经寄存器槽
 *    (rdi/rsi/rdx/rcx/r8/r9), 超出忽略。
 *
 * mcontext 布局: musl x86_64 mcontext_t.gregs[23] (glibc 兼容 REG_* 索引),
 * 由 arch/x86_64/bits/signal.h 在 _GNU_SOURCE 下定义。
 */
#define _GNU_SOURCE
#include <ucontext.h>
#include <stdarg.h>
#include <stdint.h>

extern void __uc_finish(void);

/* bcheck 感知 (弱引用, 未链接 bcheck.o 时为 no-op): 显式把协程栈登记为
 * 持久检查区, 使落在协程栈上的显式越界访问能被 -b 报出。 */
extern void __bound_add_region(void *, size_t) __attribute__((weak));

void makecontext(ucontext_t *u, void (*fn)(void), int argc, ...)
{
	va_list ap;
	int n, i;
	uintptr_t top, a, *fr;
	unsigned long r[6];

	if (!u->uc_stack.ss_sp)
		return;

	top = (uintptr_t)u->uc_stack.ss_sp + (uintptr_t)u->uc_stack.ss_size;
	/* 栈顶下方预留参数 spill 空间: tcc 生成的带参函数会把前几个参数
	 * spill 到入口 rsp 上方 (如 [rbp+0x20]), 若 rsp 紧贴栈缓冲上界会越界
	 * 写坏相邻内存 (nt64 曾因此崩溃, 见 RELEASE 2026-08-24)。顶部预留 ≥0x40。 */
	a   = ((top - 0x40) & ~(uintptr_t)15);   /* a % 16 == 0 */
	fr  = (uintptr_t *)(a + 8);
	fr[0] = (uintptr_t)__uc_finish;         /* fn 返回后的兜底 */

	va_start(ap, argc);
	n = argc < 6 ? argc : 6;
	for (i = 0; i < n; i++)
		r[i] = va_arg(ap, unsigned long);
	va_end(ap);

	/* callee-saved 归零; rbp 置为 fn 入口 rsp(协程新栈基址), 使 tcc 帧指针
	 * 开起来时 fn 首条 `push rbp` 有有效栈可写 (不可为 0, 否则帧访问即崩) */
	u->uc_mcontext.gregs[REG_RBP] = a + 8;
	u->uc_mcontext.gregs[REG_RBX] = 0;
	u->uc_mcontext.gregs[REG_R12] = 0;
	u->uc_mcontext.gregs[REG_R13] = 0;
	u->uc_mcontext.gregs[REG_R14] = 0;
	u->uc_mcontext.gregs[REG_R15] = 0;

	u->uc_mcontext.gregs[REG_RSP] = a;      /* setcontext: rsp = a+8 (fn 入口) */
	u->uc_mcontext.gregs[REG_RIP] = (uintptr_t)fn;

	/* 参数: SysV 前 6 个寄存器 rdi/rsi/rdx/rcx/r8/r9 */
	switch (n) {
	case 6: u->uc_mcontext.gregs[REG_R9]  = r[5]; /* fall through */
	case 5: u->uc_mcontext.gregs[REG_R8]  = r[4]; /* fall through */
	case 4: u->uc_mcontext.gregs[REG_RCX] = r[3]; /* fall through */
	case 3: u->uc_mcontext.gregs[REG_RDX] = r[2]; /* fall through */
	case 2: u->uc_mcontext.gregs[REG_RSI] = r[1]; /* fall through */
	case 1: u->uc_mcontext.gregs[REG_RDI] = r[0];
	default: break;
	}

	/* 登记协程栈为持久检查区 (-b 下生效; 见 __bound_add_region 弱声明注释)。 */
	if (__bound_add_region)
		__bound_add_region(u->uc_stack.ss_sp, u->uc_stack.ss_size);
}
