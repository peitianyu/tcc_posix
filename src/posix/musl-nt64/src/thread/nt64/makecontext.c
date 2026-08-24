/* makecontext(): 在给定的独立栈上布置一个入口 fn 及其寄存器参数,
 * 使后续 setcontext/swapcontext 恢复该上下文时直接跳到 fn 执行。
 *
 * 值语义 (与同目录 ucontext.s 约定一致):
 *  - setcontext/swapcontext 恢复时 rsp = 存值+8; 故存值 uc_rsp 必须 %16==0,
 *    使 fn 入口 rsp%16==8 (SysV: 等效 call 压 8B 返回地址后的对齐)。
 *  - 返回地址 __uc_finish 预放于 [存值+8], fn 正常 ret 时兜底自旋。
 *  - uc_mcontext.uc_rip = fn; 其余 GPR 槽按 SysV 传参约定由 setcontext 恢复。
 *  - 参数: makecontext(ucp, fn, argc, a1..an): 前 ≤6 个经寄存器槽
 *    (rdi/rsi/rdx/rcx/r8/r9), 暂不支持 >6 (超出忽略)。
 */
#include <ucontext.h>
#include <stdarg.h>
#include <stdint.h>

extern void __uc_finish(void);

void makecontext(ucontext_t *u, void (*fn)(void), int argc, ...)
{
	va_list ap;
	int n, i;
	uintptr_t top, a, *fr;
	unsigned long r[6];

	if (!u->uc_stack.ss_sp)
		return;

	top = (uintptr_t)u->uc_stack.ss_sp + (uintptr_t)u->uc_stack.ss_size;
	/* 最高处 16 对齐(向下), 预留返回地址槽上方余量 */
	a   = ((top - 16) & ~(uintptr_t)15);    /* a % 16 == 0 */
	fr  = (uintptr_t *)(a + 8);
	fr[0] = (uintptr_t)__uc_finish;         /* fn 返回后的兜底 */

	va_start(ap, argc);
	n = argc < 6 ? argc : 6;
	for (i = 0; i < n; i++)
		r[i] = va_arg(ap, unsigned long);
	va_end(ap);

	/* callee-saved 归零; rbp 置为 fn 入口 rsp(协程新栈基址), 使 tcc 帧指针
	 * 开起来时 fn 首条 `push rbp` 有有效栈可写 (不可为 0, 否则帧访问即崩) */
	u->uc_mcontext.uc_rbp = a + 8;
	u->uc_mcontext.uc_rbx = 0;
	u->uc_mcontext.uc_r12 = 0;
	u->uc_mcontext.uc_r13 = 0;
	u->uc_mcontext.uc_r14 = 0;
	u->uc_mcontext.uc_r15 = 0;

	/* 参数: makecontext(ucp, fn, argc, a1..an), 前 ≤4 个经 Windows x64 参数
	 * 寄存器槽 (rcx/rdx/r8/r9), >4 暂不支持 (超出忽略)。 */
	u->uc_mcontext.uc_rsp = a;             /* setcontext: rsp = a+8 (fn 入口) */
	u->uc_mcontext.uc_rip = (uintptr_t)fn;

	switch (n) {
	case 4: u->uc_mcontext.uc_r9  = r[3]; /* fall through */
	case 3: u->uc_mcontext.uc_r8  = r[2]; /* fall through */
	case 2: u->uc_mcontext.uc_rdx = r[1]; /* fall through */
	case 1: u->uc_mcontext.uc_rcx = r[0];
	default: break;
	}
}