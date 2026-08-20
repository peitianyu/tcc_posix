.text
.global	__syscall

__syscall:
	jmp	__syscall_disp

	.section .got$__syscall,"r"
	.global __imp___syscall
__imp___syscall:
	.quad	__syscall
