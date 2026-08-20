/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>

#if (__COMPILER__ == __MSVC__) && defined(__X86_MODEL)

intptr_t __fastcall __tt_fork_impl(
	__in	uintptr_t	saved_regs_stack_pointer,
	__in	uintptr_t	stack_adjustment);

int32_t __declspec(naked) __cdecl __tt_fork(void)
{
	__asm {
		push	ebp
		mov	ebp, esp

		push	ecx
		push	edx
		push	ebx
		push	esi
		push	edi

		mov	ecx, esp
		call	__tt_fork_impl

		pop	edi
		pop	esi
		pop	ebx
		pop	edx
		pop	ecx

		mov	esp, ebp
		pop	ebp
		ret
	};
}

void __declspec(naked) __fastcall __tt_fork_child_entry_point(uintptr_t esp_saved)
{
	__asm {
		xor	eax, eax
		mov	esp, ecx

		pop	edi
		pop	esi
		pop	ebx
		pop	edx
		pop	ecx

		pop	ebp
		ret
	};
}

void __declspec(naked) __fastcall __tt_fork_child_entry_point_adj(uintptr_t esp_saved)
{
	__asm {
		jmp	__tt_fork_child_entry_point
	};
}

#endif
