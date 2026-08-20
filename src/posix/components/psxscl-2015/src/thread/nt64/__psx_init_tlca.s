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
	movq	%rcx,	%rsp
	movq	%rdx,	%rax
	movq	(%rcx),	%rdx
	movq	$-2,	%rcx
	jmp		*%rax
