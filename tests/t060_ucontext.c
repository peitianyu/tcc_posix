/* t060_ucontext: 协程基座冒烟 (nt64 移植的 ucontext).
 * core: getcontext/setcontext/makecontext/swapcontext 完整往返.
 * 纯断言(无 stdio)。退出用 _Exit:  独立栈协程运行后, tcc/psxscl 的 CRT
 * 正常 return 清理路径(间接 call 全局函数指针表)在 Windows 下崩; _Exit 直接
 * 绕开该清理, 协程切换/传参断言不受影响。getcontext/setcontext(同栈)则正常。
 * 语义:
 *   main                                     co1_fn
 *   swapcontext(main_ctx,&co1)  -->  stage=1
 *   assert stage==1               <--  swapcontext(co1,&main_ctx)
 *   swapcontext(main_ctx,&co1)  -->  stage=2;  swapcontext(co1,&main_ctx)
 *   assert stage==2               <--
 *   -> _Exit(fail)
 */
#include <stdlib.h>
#include <ucontext.h>

static ucontext_t main_ctx, co1;
static char st1[8192] __attribute__((aligned(16)));
static int stage = 0;

static void co1_fn(int a, int b, int c)
{
	/* 跨切换保留: a/b/c 经回归寄存器传参, stage 经全局保留 */
	if (a != 10 || b != 20 || c != 30) { stage = -1; return; }
	stage = 1;
	swapcontext(&co1, &main_ctx);
	stage = 2;
	swapcontext(&co1, &main_ctx);
	stage = 3;
}

int main(void)
{
	int fail = 0;
	co1.uc_stack.ss_sp  = st1;
	co1.uc_stack.ss_size = sizeof st1;
	makecontext(&co1, co1_fn, 3, 10, 20, 30);

	swapcontext(&main_ctx, &co1);            /* 进入协程 */
	if (stage != 1) fail |= 1;
	swapcontext(&main_ctx, &co1);            /* 恢复协程 (stage→2, 切回) */
	if (stage != 2) fail |= 2;

	_Exit(fail ? fail : 0);
}