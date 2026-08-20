/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

TITLE __psx_tlca_init

.code
__psx_tlca_prolog PROC EXPORT
	mov		rsp, rdx
	call	rcx
	ret
__psx_tlca_prolog ENDP

__psx_tlca_epilog PROC EXPORT
	mov		rsp, rcx
	mov		rax, rdx
	mov		rdx, [rcx]
	mov		rcx, -2
	jmp		rax
__psx_tlca_epilog ENDP

END
