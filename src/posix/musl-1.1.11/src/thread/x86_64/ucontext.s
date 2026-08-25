/* ucontext 协程基座 (linux x86_64 / SysV ABI)。
 *
 * 调用约定: SysV x86_64 —— 首参 %rdi (nt64 版是 Windows x64 %rcx, 见
 * src/posix/musl-nt64/src/thread/nt64/ucontext.s)。
 *   getcontext(ucp):  ucp=%rdi
 *   setcontext(ucp):  ucp=%rdi, 永不返回
 *   swapcontext(o,u): o=%rdi, u=%rsi
 *
 * 上下文模型 (与 makecontext.c 统一步调, 同 nt64 版):
 *   getcontext(ucp):  保存当前执行现场到 ucp; RSP=调用时 %rsp (栈顶=返回地址槽),
 *                      RIP=(%rsp) 即返回地址。
 *   setcontext(ucp):  恢复现场, 永不返回; rsp=存RSP+8 (弹掉返回地址槽), jmp 存RIP。
 *   swapcontext(o,u): 先把现场存到 o (同 getcontext), 再恢复 u (用 rsi->rdi)。
 *
 * mcontext 寄存器槽偏移 (ucp 相对; musl x86_64 mcontext_t = gregs[23] long long,
 * ucontext_t 内 uc_mcontext 偏移 0x28 = uc_flags(8)+uc_link(8)+stack_t(24,
 * ss_sp 8 + ss_size 8 + ss_flags 4 + pad 4); gregs 布局同 glibc REG_*):
 *   gregs[i] @ ucp+0x28+i*8:
 *   r8=0x28 r9=0x30 r10=0x38 r11=0x40 r12=0x48 r13=0x50 r14=0x58 r15=0x60
 *   rdi=0x68 rsi=0x70 rbp=0x78 rbx=0x80 rdx=0x88 rax=0x90 rcx=0x98 rsp=0xa0 rip=0xa8
 */
.text

/* int getcontext(ucontext_t *ucp)  (%rdi) */
.globl getcontext
getcontext:
	movq %rax, 0x90(%rdi)
	movq %rdi, 0x68(%rdi)
	movq %rsi, 0x70(%rdi)
	movq %rdx, 0x88(%rdi)
	movq %rcx, 0x98(%rdi)
	movq %rbx, 0x80(%rdi)
	movq %rsp, 0xa0(%rdi)
	movq %rbp, 0x78(%rdi)
	movq %r8,  0x28(%rdi)
	movq %r9,  0x30(%rdi)
	movq %r10, 0x38(%rdi)
	movq %r11, 0x40(%rdi)
	movq %r12, 0x48(%rdi)
	movq %r13, 0x50(%rdi)
	movq %r14, 0x58(%rdi)
	movq %r15, 0x60(%rdi)
	movq (%rsp), %rax
	movq %rax, 0xa8(%rdi)     /* rip = 返回地址 */
	xor %eax, %eax
	ret

/* int setcontext(const ucontext_t *ucp) — 永不返回 (%rdi) */
.globl setcontext
setcontext:
	jmp .Lrestore

/* int swapcontext(ucontext_t *old, const ucontext_t *new)
 *   old=%rdi, new=%rsi (SysV) */
.globl swapcontext
swapcontext:
	movq %rax, 0x90(%rdi)
	movq %rdi, 0x68(%rdi)
	movq %rsi, 0x70(%rdi)
	movq %rdx, 0x88(%rdi)
	movq %rcx, 0x98(%rdi)
	movq %rbx, 0x80(%rdi)
	movq %rsp, 0xa0(%rdi)
	movq %rbp, 0x78(%rdi)
	movq %r8,  0x28(%rdi)
	movq %r9,  0x30(%rdi)
	movq %r10, 0x38(%rdi)
	movq %r11, 0x40(%rdi)
	movq %r12, 0x48(%rdi)
	movq %r13, 0x50(%rdi)
	movq %r14, 0x58(%rdi)
	movq %r15, 0x60(%rdi)
	movq (%rsp), %rax
	movq %rax, 0xa8(%rdi)     /* rip = 返回地址 */
	movq %rsi, %rdi            /* rdi = new ucp */
	/* fall through 恢复 */

.Lrestore:
	/* 先把 target rdi 暂存(它是 base, 负载完才恢复), 再暂存 target rip/rsp。
	 * 栈(顶->下): [rsp][rip][rdi] */
	movq 0x68(%rdi), %r8
	pushq %r8                  /* target rdi  栈[16] */
	movq 0xa8(%rdi), %r9
	pushq %r9                  /* target rip  栈[8] */
	movq 0xa0(%rdi), %r10
	pushq %r10                 /* target rsp  栈[0] */

	movq 0x28(%rdi), %r8
	movq 0x30(%rdi), %r9
	movq 0x38(%rdi), %r10
	movq 0x40(%rdi), %r11
	movq 0x48(%rdi), %r12
	movq 0x50(%rdi), %r13
	movq 0x58(%rdi), %r14
	movq 0x60(%rdi), %r15
	movq 0x70(%rdi), %rsi
	movq 0x78(%rdi), %rbp
	movq 0x80(%rdi), %rbx
	movq 0x88(%rdi), %rdx
	movq 0x90(%rdi), %rax
	movq 0x98(%rdi), %rcx

	/* 逐栈恢复: 16=target rdi(base 已用完), 8=target rip, 0=target rsp */
	movq 16(%rsp), %rdi
	movq 8(%rsp),  %r11
	movq (%rsp),   %r10
	addq $8, %r10              /* rsp = 存值+8 (弹返回地址槽) */
	movq %r10, %rsp
	jmp *%r11

/* fn 若意外返回的兜底: 自旋 (协程体应始终以 swapcontext 让出) */
.globl __uc_finish
__uc_finish:
1:	jmp 1b
