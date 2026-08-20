##########################################################
##  psxscl: a thread-safe system call layer library     ##
##  Copyright (C) 2013,2014,2015  Z. Gilboa             ##
##  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. ##
##########################################################

.section .text

.global ___psx_tlca_prolog
.global ___psx_tlca_epilog

___psx_tlca_prolog:
	movq	%edx, %esp	# switch stacks
	call	*%ecx		# call the 'real' init routine
	ret

___psx_tlca_epilog:
	movq	%ecx,	%esp
	movq	%edx,	%eax
	movq	(%ecx),	%edx
	movq	$-2,	%ecx
	jmp		*%eax
