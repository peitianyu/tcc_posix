/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#ifndef ___NTAPI_CONTEXT_H_
#define ___NTAPI_CONTEXT_H_

#if defined(__X86_MODEL)
	/* csr port handle */
	#define __GET_CSR_PORT_HANDLE_BY_LOGIC	__ntapi_tt_get_csr_port_handle_addr_by_logic_i386

	/* register names */
	#define STACK_POINTER_REGISTER		uc_esp
	#define INSTRUCTION_POINTER_REGISTER	uc_eip
	#define FAST_CALL_ARG0			uc_ecx
	#define FAST_CALL_ARG1			uc_edx

	/* thread context initialization */
	#define __INIT_CONTEXT(context)	\
			context.uc_context_flags = NT_CONTEXT_JUST_EVERYTHING; \
			context.uc_seg_gs	= 0x00;	\
			context.uc_seg_fs	= 0x3b;	\
			context.uc_seg_es	= 0x23;	\
			context.uc_seg_ds	= 0x23;	\
			context.uc_seg_ss	= 0x23;	\
			context.uc_seg_cs	= 0x1b;	\
			context.uc_eflags	= 0x200

#elif defined (__X86_64_MODEL)
	/* csr port handle */
	#define __GET_CSR_PORT_HANDLE_BY_LOGIC	__ntapi_tt_get_csr_port_handle_addr_by_logic_x86_64

	/* register names */
	#define STACK_POINTER_REGISTER		uc_rsp
	#define INSTRUCTION_POINTER_REGISTER	uc_rip
	#define FAST_CALL_ARG0			uc_rcx
	#define FAST_CALL_ARG1			uc_rdx

	/* thread context initialization */
	#define __INIT_CONTEXT(context)	\
			context.uc_context_flags= NT_CONTEXT_JUST_EVERYTHING; \
			context.uc_seg_cs	= 0x33;	\
			context.uc_seg_ds	= 0x2b;	\
			context.uc_seg_es	= 0x2b;	\
			context.uc_seg_fs	= 0x53;	\
			context.uc_seg_gs	= 0x2b;	\
			context.uc_seg_ss	= 0x2b;	\
			context.uc_eflags	= 0x200; \
			context.uc_mx_csr	= 0x1f80

#endif

#endif
