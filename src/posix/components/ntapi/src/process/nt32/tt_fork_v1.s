##########################################################
##  ntapi: Native API core library                      ##
##  Copyright (C) 2013,2014,2015  Z. Gilboa             ##
##  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  ##
##########################################################

.section .text

.global ___tt_fork
.global ___tt_fork_child_entry_point
.global @__tt_fork_child_entry_point@4
.global ___tt_fork_child_entry_point_adj
.global @__tt_fork_child_entry_point_adj@4

___tt_fork:
___tt_fork_prolog:
	push	%ebp
	mov	%esp, %ebp

___tt_fork_save_regs:
	push	%ecx
	push	%edx
	push	%ebx
	push	%esi
	push	%edi

___tt_fork_impl_call:
	mov	%esp, %ecx
	mov	$0,   %edx
	call @__tt_fork_impl@8

___tt_fork_restore_regs:
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%edx
	pop	%ecx

___tt_fork_epilog:
	mov	%ebp, %esp
	pop	%ebp
	ret

___tt_fork_child_entry_point:
@__tt_fork_child_entry_point@4:
___tt_fork_child_entry_point_adj:
@__tt_fork_child_entry_point_adj@4:
	xor	%eax, %eax
	mov	%ecx, %esp

___tt_fork_child_restore_regs:
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%edx
	pop	%ecx

___tt_fork_child_epilog:
	pop	%ebp
	ret
