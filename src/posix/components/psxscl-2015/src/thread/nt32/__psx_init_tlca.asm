/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

TITLE __psx_tlca_init

.code
___psx_tlca_prolog PROC EXPORT
	mov		esp, edx
	call	ecx
	ret
___psx_tlca_prolog ENDP

___psx_tlca_epilog PROC EXPORT
	mov		esp, ecx
	mov		eax, edx
	mov		edx, [ecx]
	mov		ecx, -2
	jmp		eax
___psx_tlca_epilog ENDP

END
