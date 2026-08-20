#include "psx_systypes.h"

struct pt_regs {
	intptr_t	ebx;
	intptr_t	ecx;
	intptr_t	edx;
	intptr_t	esi;
	intptr_t	edi;
	intptr_t	ebp;
	intptr_t	eax;
	intptr_t 	xds;
	intptr_t 	xes;
	intptr_t	xfs;
	intptr_t	xgs;
	intptr_t	orig_eax;
	intptr_t	eip;
	intptr_t	xcs;
	intptr_t	eflags;
	intptr_t	esp;
	intptr_t	xss;
};
