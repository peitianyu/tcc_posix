//#########################################################
//#  ntapi: Native API core library                      ##
//#  Copyright (C) 2013,2014,2015  Z. Gilboa             ##
//#  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  ##
//#########################################################

.section .text

.global __tt_fork_v1
.global __tt_fork_child_entry_point
.global __tt_fork_child_entry_point_adj

__tt_fork_v1:
__tt_fork_save_regs:
	push	%rbp
	push	%rcx
	push	%rdx
	push	%rbx
	push	%rsi
	push	%rdi
	push	%r8
	push	%r9
	push	%r10
	push	%r11
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	sub 0x40,%rsp

	mov	%rsp, %rdx
	and     $0xf, %rdx
	test	%rdx, %rdx
	jne	__tt_fork_impl_adj_call

__tt_fork_impl_call:
	mov	%rsp, %rcx
	call __tt_fork_impl_v1

	add 0x40,%rsp

	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rbx
	pop	%rdx
	pop	%rcx
	pop	%rbp

	ret

__tt_fork_impl_adj_call:
	push %rdi

	mov	%rsp, %rcx
	call __tt_fork_impl_v1

	pop	%rdi

	add 0x40,%rsp

	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rbx
	pop	%rdx
	pop	%rcx
	pop	%rbp

	ret


__tt_fork_child_entry_point:
	xor	%rax, %rax
	mov	%rcx, %rsp

	add 0x40,%rsp

	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rbx
	pop	%rdx
	pop	%rcx
	pop	%rbp

	ret

__tt_fork_child_entry_point_adj:
	xor	%rax, %rax
	mov	%rcx, %rsp

	pop	%rdi

	add 0x40,%rsp

	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rbx
	pop	%rdx
	pop	%rcx
	pop	%rbp

	ret
