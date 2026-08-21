/* tcc_posix: CreateThread 调用包装

   背景: TCC 对 PE 目标 (TCC_TARGET_PE) 使用 Win64 调用约定
   (rcx/rdx/r8/r9 + 32 字节 shadow space + 栈参从 rsp+40 起),
   与 kernel32/ntdll 原生 API 完全兼容, 理论上可直接调用。
   此包装保留作防御: 把 fn 移到 r11 后按 Win64 重排参数再 call,
   对 Win64 调用者等价于恒等变换 (参考 cosmopolitan __sysv2nt
   系列 — 但 cosmopolitan 全程 SysV ABI, 我们 TCC PE 已是 Win64)。

   rcx=fn, rdx=a1, r8=a2, r9=a3, [rsp+40]=a4, [rsp+48]=a5, [rsp+56]=a6 */
#if defined(__x86_64__)
	.text
	.globl	psx_sysv2nt6
psx_sysv2nt6:
	push	%rbp
	mov	%rsp,%rbp
	sub	$48,%rsp
	mov	%rcx,%r11		/* fn → r11 (跨调用保留) */
	mov	%rdx,%rcx		/* a1 → rcx */
	mov	%r8,%rdx		/* a2 → rdx */
	mov	%r9,%r8			/* a3 → r8 */
	mov	48(%rbp),%r9		/* a4 → r9 */
	mov	56(%rbp),%rax		/* a5 */
	mov	%rax,32(%rsp)
	mov	64(%rbp),%rax		/* a6 */
	mov	%rax,40(%rsp)
	call	*%r11
	leave
	ret
#endif
