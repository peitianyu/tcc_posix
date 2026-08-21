//#########################################################
//#  psxscl: a thread-safe system call layer library     ##
//#  Copyright (C) 2013,2014,2015  Z. Gilboa             ##
//#  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. ##
//#########################################################

.section .text

.global __psx_tlca_prolog
.global __psx_tlca_epilog

__psx_tlca_prolog:
	movq	%rdx, %rsp	# switch stacks
	call	*%rcx		# call the 'real' init routine
	ret

__psx_tlca_epilog:
	/* tcc_posix: 不切栈 (切到 stack_limit 后 ZwTerminateThread 的
	   syscall stub 若失败会 ret, 从栈底读返回地址 → 崩/进程消失;
	   worker 栈未释放, 直接用它调 ZwTerminateThread 安全) */
	movq	%rdx,	%rax
	movq	(%rcx),	%rdx
	movq	$-2,	%rcx
	jmp		*%rax
