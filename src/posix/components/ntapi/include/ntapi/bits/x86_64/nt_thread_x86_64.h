#ifndef _NT_THREAD_X86_64_H_
#define _NT_THREAD_X86_64_H_

#include <psxtypes/psxtypes.h>

typedef struct {
	uintptr_t	uc_low;
	intptr_t	uc_high;
} nt_m128a_t;

typedef struct {
	uint16_t		uc_control_word;		/* 0x000 */
	uint16_t		uc_status_word;			/* 0x002 */
	uint8_t			uc_tag_word;			/* 0x004 */
	uint8_t			uc_reserved1;			/* 0x005 */
	uint16_t		uc_error_opcode;		/* 0x006 */
	uint32_t		uc_error_offset;		/* 0x008 */
	uint16_t		uc_error_selector;		/* 0x00c */
	uint16_t		uc_reserved2;			/* 0x00e */
	uint32_t		uc_data_offset;			/* 0x010 */
	uint16_t		uc_data_selector;		/* 0x014 */
	uint16_t		uc_reserved3;			/* 0x016 */
	uint32_t		uc_mx_csr;			/* 0x018 */
	uint32_t		uc_mx_csr_mask;			/* 0x01c */
	nt_m128a_t		uc_float_registers[8];		/* 0x020 */
	nt_m128a_t		uc_xmm_registers[16];		/* 0x0a0 */
	uint8_t			uc_reserved4[96];		/* 0x1a0 */
} nt_xsave_fmt_t;

typedef struct {
	uintptr_t		uc_p1_home;			/* 0x000 */
	uintptr_t		uc_p2_home;			/* 0x008 */
	uintptr_t		uc_p3_home;			/* 0x010 */
	uintptr_t		uc_p4_home;			/* 0x018 */
	uintptr_t		uc_p5_home;			/* 0x020 */
	uintptr_t		uc_p6_home;			/* 0x028 */
	uint32_t		uc_context_flags;		/* 0x030 */
	uint32_t		uc_mx_csr;			/* 0x034 */
	uint16_t		uc_seg_cs;			/* 0x038 */
	uint16_t		uc_seg_ds;			/* 0x03a */
	uint16_t		uc_seg_es;			/* 0x03c */
	uint16_t		uc_seg_fs;			/* 0x03e */
	uint16_t		uc_seg_gs;			/* 0x040 */
	uint16_t		uc_seg_ss;			/* 0x042 */
	uint32_t		uc_eflags;			/* 0x044 */
	uintptr_t		uc_dr0;				/* 0x048 */
	uintptr_t		uc_dr1;				/* 0x050 */
	uintptr_t		uc_dr2;				/* 0x058 */
	uintptr_t		uc_dr3;				/* 0x060 */
	uintptr_t		uc_dr6;				/* 0x068 */
	uintptr_t		uc_dr7;				/* 0x070 */
	uintptr_t		uc_rax;				/* 0x078 */
	uintptr_t		uc_rcx;				/* 0x080 */
	uintptr_t		uc_rdx;				/* 0x088 */
	uintptr_t		uc_rbx;				/* 0x090 */
	uintptr_t		uc_rsp;				/* 0x098 */
	uintptr_t		uc_rbp;				/* 0x0a0 */
	uintptr_t		uc_rsi;				/* 0x0a8 */
	uintptr_t		uc_rdi;				/* 0x0b0 */
	uintptr_t		uc_r8;				/* 0x0b8 */
	uintptr_t		uc_r9;				/* 0x0c0 */
	uintptr_t		uc_r10;				/* 0x0c8 */
	uintptr_t		uc_r11;				/* 0x0d0 */
	uintptr_t		uc_r12;				/* 0x0d8 */
	uintptr_t		uc_r13;				/* 0x0e0 */
	uintptr_t		uc_r14;				/* 0x0e8 */
	uintptr_t		uc_r15;				/* 0x0f0 */
	uintptr_t		uc_rip;				/* 0x0f8 */

	union {
		nt_xsave_fmt_t		uc_flt_save;		/* 0x100 */

		struct {
			nt_m128a_t	uc_header[2];		/* 0x100 */
			nt_m128a_t	uc_legacy[8];		/* 0x120 */
		} uc_hdr;
	} uc_flt;

	nt_m128a_t		uc_xmm0;			/* 0x1a0 */
	nt_m128a_t		uc_xmm1;			/* 0x1b0 */
	nt_m128a_t		uc_xmm2;			/* 0x1c0 */
	nt_m128a_t		uc_xmm3;			/* 0x1d0 */
	nt_m128a_t		uc_xmm4;			/* 0x1e0 */
	nt_m128a_t		uc_xmm5;			/* 0x1f0 */
	nt_m128a_t		uc_xmm6;			/* 0x200 */
	nt_m128a_t		uc_xmm7;			/* 0x210 */
	nt_m128a_t		uc_xmm8;			/* 0x220 */
	nt_m128a_t		uc_xmm9;			/* 0x230 */
	nt_m128a_t		uc_xmm10;			/* 0x240 */
	nt_m128a_t		uc_xmm11;			/* 0x250 */
	nt_m128a_t		uc_xmm12;			/* 0x260 */
	nt_m128a_t		uc_xmm13;			/* 0x270 */
	nt_m128a_t		uc_xmm14;			/* 0x280 */
	nt_m128a_t		uc_xmm15;			/* 0x290 */
	nt_m128a_t		uc_vector_register[26];		/* 0x300 */
	uintptr_t		uc_vector_control;		/* 0x4a0 */
	uintptr_t		uc_debug_control;		/* 0x4a8 */
	uintptr_t		uc_last_branch_to_rip;		/* 0x4b0 */
	uintptr_t		uc_last_branch_from_rip;	/* 0x4b8 */
	uintptr_t		uc_last_exception_to_rip;	/* 0x4c0 */
	uintptr_t		uc_last_exception_from_rip;	/* 0x4c8 */
} nt_mcontext_x86_64_t;

#endif
