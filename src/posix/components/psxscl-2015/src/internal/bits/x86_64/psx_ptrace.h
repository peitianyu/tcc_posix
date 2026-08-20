#include "psx_systypes.h" 

struct pt_regs {
	uintptr_t	r15;
	uintptr_t	r14;
	uintptr_t	r13;
	uintptr_t	r12;
	uintptr_t	rbp;
	uintptr_t	rbx;
	uintptr_t	r11;
	uintptr_t	r10;
	uintptr_t	r9;
	uintptr_t	r8;
	uintptr_t	rax;
	uintptr_t	rcx;
	uintptr_t	rdx;
	uintptr_t	rsi;
	uintptr_t	rdi;
	uintptr_t	orig_rax;
	uintptr_t	rip;
	uintptr_t	cs;
	uintptr_t	eflags;
	uintptr_t	rsp;
	uintptr_t	ss;
};
