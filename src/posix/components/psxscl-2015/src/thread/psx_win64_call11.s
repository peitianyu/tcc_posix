/* tcc_posix: 11 参 Win64 调用 (NtCreateThreadEx)
   rcx=fn, rdx=a1, r8=a2, r9=a3, [rsp+40..]=a4..a11 */
#if defined(__x86_64__)
	.text
	.globl	psx_sysv2nt11
psx_sysv2nt11:
	push	%rbp
	mov	%rsp,%rbp
	sub	$120,%rsp
	mov	%rcx,%r11		/* fn */
	mov	%rdx,%rcx		/* a1 */
	mov	%r8,%rdx		/* a2 */
	mov	%r9,%r8			/* a3 */
	/* 栈参 a4..a11 从 [rbp+48] 起 (调用者 [rsp+40], push rbp 后 +8) */
	mov	48(%rbp),%r9		/* a4 */
	mov	56(%rbp),%rax		/* a5 */
	mov	%rax,32(%rsp)
	mov	64(%rbp),%rax		/* a6 */
	mov	%rax,40(%rsp)
	mov	72(%rbp),%rax		/* a7 */
	mov	%rax,48(%rsp)
	mov	80(%rbp),%rax		/* a8 */
	mov	%rax,56(%rsp)
	mov	88(%rbp),%rax		/* a9 */
	mov	%rax,64(%rsp)
	mov	96(%rbp),%rax		/* a10 */
	mov	%rax,72(%rsp)
	mov	104(%rbp),%rax		/* a11 */
	mov	%rax,80(%rsp)
	call	*%r11
	leave
	ret
#endif
