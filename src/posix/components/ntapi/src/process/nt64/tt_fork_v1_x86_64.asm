/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

TITLE tt_fork_x86_64

.data
__tt_fork_impl_v1 PROTO C

.code
__tt_fork_v1 PROC
	push	rbp
	push	rcx
	push	rdx
	push	rbx
	push	rsi
	push	rdi
	push	r8
	push	r9
	push	r10
	push	r11
	push	r12
	push	r13
	push	r14
	push	r15

	sub rsp, 40h

	mov  rdx, rsp
	and  rdx, 15
	test rdx, rdx
	jne	__tt_fork_impl_adj_call

	mov	rcx, rsp
	call __tt_fork_impl_v1

	add rsp, 40h

	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	r11
	pop	r10
	pop	r9
	pop	r8
	pop	rdi
	pop	rsi
	pop	rbx
	pop	rdx
	pop	rcx
	pop	rbp
	ret
__tt_fork_v1 ENDP

__tt_fork_impl_adj_call PROC
	push rdi

	mov	rcx, rsp
	mov	rdx, 1
	call __tt_fork_impl_v1

	pop	rdi

	add rsp, 40h

	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	r11
	pop	r10
	pop	r9
	pop	r8
	pop	rdi
	pop	rsi
	pop	rbx
	pop	rdx
	pop	rcx
	pop	rbp
	ret
__tt_fork_impl_adj_call ENDP


__tt_fork_child_entry_point PROC
	xor	rax, rax
	mov	rsp, rcx

	add rsp, 40h

	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	r11
	pop	r10
	pop	r9
	pop	r8
	pop	rdi
	pop	rsi
	pop	rbx
	pop	rdx
	pop	rcx
	pop	rbp
	ret
__tt_fork_child_entry_point ENDP


__tt_fork_child_entry_point_adj PROC
	xor	rax, rax
	mov	rsp, rcx

	pop	rdi

	add rsp, 40h

	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	r11
	pop	r10
	pop	r9
	pop	r8
	pop	rdi
	pop	rsi
	pop	rbx
	pop	rdx
	pop	rcx
	pop	rbp
	ret
__tt_fork_child_entry_point_adj ENDP

END
