/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

TITLE tt_fork_x86_64

.data
__tt_fork_impl_v2 PROTO C

.code
__tt_fork_v2 PROC
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
	call __tt_fork_impl_v2
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
__tt_fork_v2 ENDP

END
