/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

/************************************************************/
/* beginning with version 6.0, explicit thread registration */
/* with csrss is no longer required.  the code below should */
/* work with all known versions of NT, however it will only */
/* be used when run on the now-obsolete versions of the OS. */
/************************************************************/

/**
   Nebbett was pretty much right in his interpretation of
   the csrss port message; and as long as one changes
   uint32_t to uintptr_t (especially when it comes to the
   unknown parameters), then the structures behave as
   expected according to his book.

   SysInternals: ProcessExplorer: csrss.exe: the stack shows
   a thread in csrsrv.dll that has CsrUnhandledExceptionFilter
   as its start address, and ntdll!NtReplyWaitReceivePort as
   its next function call.  This suggests that csrss still
   uses LPC (at least to some extent) for communication with
   user processes.

   Given the above, we may deduce that CsrClientCallServer
   contains a call to ZwRequestWaitReplyPort.  Assuming
   the machine code in ntdll is as optimized as possible,
   we may then conclude that on x86 machines, this would be
   an E8 call using relative 32-bit addressing on both NT32
   and NT64.

   On the 32-bit variant of the operating system, the first
   argument is passed on the stack, and is normally expressed
   in terms of an offset from the ds register.

   On the 64-bit variant of the operating system, the first
   argument is passed in the rcx register.  Here, again,
   machine code optimization suggests that the address of
   CsrPortHandle will be provided as a 32-bit relative address,
   or else the code will be larger by several bytes.

   The rest is based on simple logic and straight-forward
   heuristics.  Since we know the addresses of CsrClientCallSertver
   and ZwRequestWaitReplyPort, we first find the call to the latter
   function within the former.  Once we have found that call, we
   start going back to look for the argument-passing
   opcode, and finally do the math to obtain the address of
   CsrPortHandle.
**/


#if defined(__X86_MODEL)
void ** __cdecl __ntapi_tt_get_csr_port_handle_addr_by_logic_i386(void)
{
	#define MAX_BYTES_BETWEEN_ARG1_PUSH_AND_E8_CALL 0x20
	#define MAX_FN_BYTES_TO_TEST			0x800

	typedef struct __attr_aligned__ (1) __attr_packed__ __x86_e8_call_signature {
		unsigned char	__opcode_current_e8;
		unsigned char	__addr_relative[4];
		unsigned char	__opcode_next_any;
	} _x86_e8_call_signature;

	typedef struct __attr_aligned__ (1) __attr_packed__ __x86_push_ds_signature {
		unsigned char	__push;
		unsigned char	__ds;
		unsigned char	__push_ds_arg;
	} _x86_push_ds_signature;

	unsigned char *			ptr_test;
	_x86_e8_call_signature *	ptr_e8_call;
	_x86_push_ds_signature *	ptr_push_ds;
	int32_t				offset;

	/* type-punned tyrants */
	int32_t *			prelative;
	int32_t				relative;
	uintptr_t *			pport_addr;


	/* calling a function within the same library: assume E8 call */
	for (offset = 0; offset < MAX_FN_BYTES_TO_TEST; offset++) {
		ptr_test = (unsigned char *)__ntapi->csr_client_call_server
				+ offset;

		if (*ptr_test == 0xE8) {
			ptr_e8_call = (_x86_e8_call_signature *)ptr_test;

			/* make our type-punned tyrant compiler happy */
			prelative = (int32_t *)&(ptr_e8_call->__addr_relative);
			relative = *prelative;

			/* are we calling ZwRequestWaitReplyPort? */
			if ((uintptr_t)(__ntapi->zw_request_wait_reply_port) ==
					(uintptr_t)&(ptr_e8_call->__opcode_next_any)
					+ relative) {
				/* assume ds relative address for arg1, go back to find it */
				for (offset = 0; offset < MAX_BYTES_BETWEEN_ARG1_PUSH_AND_E8_CALL; offset++) {
					ptr_push_ds = (_x86_push_ds_signature *)((uintptr_t)ptr_e8_call - offset);

					if ((ptr_push_ds->__push == 0xFF) &&
							(ptr_push_ds->__ds == 0x35)) {
						/* bingo */
						/* make our type-punned tyrant compiler happy */
						pport_addr = (uintptr_t *)&(ptr_push_ds->__push_ds_arg);

						/* all done */
						return *(void ***)pport_addr;
					}
				}
			}
		}
	}

	/* CsrPortHandle not found */
	return (void **)0;
}
#endif


#if defined(__X86_64_MODEL)
void ** __ntapi_tt_get_csr_port_handle_addr_by_logic_x86_64(void)
{
	#define MAX_BYTES_BETWEEN_ARG1_PUSH_AND_E8_CALL 0x20
	#define MAX_FN_BYTES_TO_TEST			0x800

	typedef struct __attr_aligned__ (1) __attr_packed__ __x86_e8_call_signature {
		unsigned char	__opcode_current_e8;
		unsigned char	__addr_relative[4];
		unsigned char	__opcode_next_any;
	} _x86_e8_call_signature;

	typedef struct __attr_aligned__ (1) __attr_packed__ __x86_move_rcx_rel_signature {
		unsigned char	__move;
		unsigned char	__rcx;
		unsigned char	__relative;
		unsigned char	__arg_32_relative[4];
		unsigned char	__opcode_next_any;
	} _x86_move_rcx_rel_signature;

	unsigned char *			ptr_test;
	_x86_e8_call_signature *	ptr_e8_call;
	_x86_move_rcx_rel_signature *	ptr_move_rcx_rel;
	int32_t				offset;
	int32_t				relative;
	int32_t *			prelative; /* for type-punned tyrants */


	/* calling a function within the same library: assume E8 call and 32-bit relative addressing */
	for (offset = 0; offset < MAX_FN_BYTES_TO_TEST; offset++) {
		ptr_test = (unsigned char *)__ntapi->csr_client_call_server
				+ offset;

		if (*ptr_test == 0xE8) {
			ptr_e8_call = (_x86_e8_call_signature *)ptr_test;

			/* please our type-punned tyrant compiler */
			prelative = (int32_t *)&(ptr_e8_call->__addr_relative);
			relative = *prelative;

			/* are we calling ZwRequestWaitReplyPort? */
			/* comparing, not writing; ignore type-punned msgs. */
			if ((uintptr_t)(__ntapi->zw_request_wait_reply_port) ==
					(uintptr_t)&(ptr_e8_call->__opcode_next_any)
					+ relative) {
				/* arg1 must be passed in rcx, so go back to find it */
				for (offset = 0; offset < MAX_BYTES_BETWEEN_ARG1_PUSH_AND_E8_CALL; offset++) {
					ptr_move_rcx_rel = (_x86_move_rcx_rel_signature *)((uintptr_t)ptr_e8_call - offset);

					if ((ptr_move_rcx_rel->__move == 0x48) &&
							(ptr_move_rcx_rel->__rcx == 0x8b) &&
							(ptr_move_rcx_rel->__relative == 0x0d))
						/* bingo */
						/* make our type-punned tyrant compiler happy */
						prelative = (int32_t *)&(ptr_move_rcx_rel->__arg_32_relative);
						relative  = *prelative;

						/* all done */
						return (void **)(
							(uintptr_t)&ptr_move_rcx_rel->__opcode_next_any
							+ relative);
				}
			}
		}
	}

	/* CsrPortHandle not found */
	return (void **)0;
}
#endif
