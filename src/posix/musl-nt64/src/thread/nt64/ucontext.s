/* ucontext 协程基座 (nt64 / x86_64)。
 *
 * 调用约定: tcc 的 PE x86_64 目标是 Windows x64 —— **首参在 %rcx** (非 SysV %rdi),
 * 与 nt64 setjmp.s 一致。故 getcontext/setcontext 以 %rcx 为 ucp;
 * swapcontext(old,%rcx, new,%rdx)。
 *
 * 上下文模型 (与 makecontext.c 统一步调):
 *   getcontext(ucp):  保存当前执行现场到 ucp; RSP=调用时 %rsp (栈顶=返回地址槽),
 *                      RIP=(%rsp) 即返回地址。
 *   setcontext(ucp):  恢复现场, 永不返回; rsp=存RSP+8 (弹掉返回地址槽), jmp 存RIP,
 *                     = 等价于该点 `getcontext 返回后`状态。
 *   swapcontext(o,u): 先把现场存到 o (同 getcontext), 再恢复 u (用 rdx->rcx)。
 *   makecontext     C 实现, 按 Windows x64 参数寄存器布置 fn 入口。
 *
 * mcontext 寄存器槽偏移 (ucp 相对, arch/nt64/bits/signal.h 布局实测):
 *   rax=0x278 rcx=0x280 rdx=0x288 rbx=0x290 rsp=0x298 rbp=0x2a0 rsi=0x2a8
 *   rdi=0x2b0 r8=0x2b8 r9=0x2c0 r10=0x2c8 r11=0x2d0 r12=0x2d8 r13=0x2e0
 *   r14=0x2e8 r15=0x2f0 rip=0x2f8
 */
.text

/* int getcontext(ucontext_t *ucp)  (%rcx) */
.globl getcontext
getcontext:
	movq %rax, 0x278(%rcx)
	movq %rcx, 0x280(%rcx)
	movq %rdx, 0x288(%rcx)
	movq %rbx, 0x290(%rcx)
	movq %rsp, 0x298(%rcx)
	movq %rbp, 0x2a0(%rcx)
	movq %rsi, 0x2a8(%rcx)
	movq %rdi, 0x2b0(%rcx)
	movq %r8,  0x2b8(%rcx)
	movq %r9,  0x2c0(%rcx)
	movq %r10, 0x2c8(%rcx)
	movq %r11, 0x2d0(%rcx)
	movq %r12, 0x2d8(%rcx)
	movq %r13, 0x2e0(%rcx)
	movq %r14, 0x2e8(%rcx)
	movq %r15, 0x2f0(%rcx)
	movq (%rsp), %rax
	movq %rax, 0x2f8(%rcx)     /* rip = 返回地址 */
	xor %eax, %eax
	ret

/* int setcontext(const ucontext_t *ucp) — 永不返回 (%rcx) */
.globl setcontext
setcontext:
	jmp .Lrestore

/* int swapcontext(ucontext_t *old, const ucontext_t *new)
 *   old=%rcx, new=%rdx (Windows x64) */
.globl swapcontext
swapcontext:
	movq %rax, 0x278(%rcx)
	movq %rcx, 0x280(%rcx)
	movq %rdx, 0x288(%rcx)
	movq %rbx, 0x290(%rcx)
	movq %rsp, 0x298(%rcx)
	movq %rbp, 0x2a0(%rcx)
	movq %rsi, 0x2a8(%rcx)
	movq %rdi, 0x2b0(%rcx)
	movq %r8,  0x2b8(%rcx)
	movq %r9,  0x2c0(%rcx)
	movq %r10, 0x2c8(%rcx)
	movq %r11, 0x2d0(%rcx)
	movq %r12, 0x2d8(%rcx)
	movq %r13, 0x2e0(%rcx)
	movq %r14, 0x2e8(%rcx)
	movq %r15, 0x2f0(%rcx)
	movq (%rsp), %rax
	movq %rax, 0x2f8(%rcx)     /* rip = 返回地址 */
	movq %rdx, %rcx            /* rcx = new ucp */
	/* fall through 恢复 */

.Lrestore:
	/* 先把 target rcx 暂存(它是 base, 负载完才恢复), 再暂存 target rip/rsp。
	 * 栈(顶->下): [rsp][rip][rcx] */
	movq 0x280(%rcx), %r8
	pushq %r8                  /* target rcx  栈[16] */
	movq 0x2f8(%rcx), %r9
	pushq %r9                  /* target rip  栈[8] */
	movq 0x298(%rcx), %r10
	pushq %r10                 /* target rsp  栈[0] */

	movq 0x278(%rcx), %rax
	movq 0x288(%rcx), %rdx
	movq 0x290(%rcx), %rbx
	movq 0x2a0(%rcx), %rbp
	movq 0x2a8(%rcx), %rsi
	movq 0x2b0(%rcx), %rdi
	movq 0x2b8(%rcx), %r8
	movq 0x2c0(%rcx), %r9
	movq 0x2c8(%rcx), %r10
	movq 0x2d0(%rcx), %r11
	movq 0x2d8(%rcx), %r12
	movq 0x2e0(%rcx), %r13
	movq 0x2e8(%rcx), %r14
	movq 0x2f0(%rcx), %r15

	/* 逐栈恢复: 16=target rcx(base 已用完), 8=target rip, 0=target rsp */
	movq 16(%rsp), %rcx
	movq 8(%rsp),  %r11
	movq (%rsp),   %r10
	addq $8, %r10              /* rsp = 存值+8 (弹返回地址槽) */
	movq %r10, %rsp
	jmp *%r11

/* fn 若意外返回的兜底: 自旋 (协程体应始终以 swapcontext 让出) */
.globl __uc_finish
__uc_finish:
1:	jmp 1b