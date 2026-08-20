#include <psxtypes/psxtypes.h>

typedef struct _nt_floating_save_area_i386 {
	uint32_t	uc_ctrl_word;		/* 0x000 */
	uint32_t	uc_status_word;		/* 0x004 */
	uint32_t	uc_tag_word;		/* 0x008 */
	uint32_t	uc_error_offset;	/* 0x00c */
	uint32_t	uc_error_selector;	/* 0x010 */
	uint32_t	uc_data_offset;		/* 0x014 */
	uint32_t	uc_data_selector;	/* 0x018 */
	unsigned char 	uc_reg_area[80];	/* 0x01c */
	uint32_t	uc_cr0_npx_state;	/* 0x06c */
} nt_floating_save_area_i386;


typedef struct _nt_thread_context_i386 {
	uint32_t	uc_context_flags;	/* 0x000 */
	uint32_t	uc_dr0;			/* 0x004 */
	uint32_t	uc_dr1;			/* 0x008 */
	uint32_t	uc_dr2;			/* 0x00c */
	uint32_t	uc_dr3;			/* 0x010 */
	uint32_t	uc_dr6;			/* 0x014 */
	uint32_t	uc_dr7;			/* 0x018 */

	nt_floating_save_area_i386
			uc_float_save;		/* 0x01c */

	uint32_t	uc_seg_gs; 		/* 0x08c */
	uint32_t	uc_seg_fs; 		/* 0x090 */
	uint32_t	uc_seg_es;		/* 0x094 */
	uint32_t	uc_seg_ds;		/* 0x098 */
	uint32_t	uc_edi;			/* 0x09c */
	uint32_t	uc_esi;			/* 0x0a0 */
	uint32_t	uc_ebx;			/* 0x0a4 */
	uint32_t	uc_edx;			/* 0x0a8 */
	uint32_t	uc_ecx;			/* 0x0ac */
	uint32_t	uc_eax;			/* 0x0b0 */
	uint32_t	uc_ebp;			/* 0x0b4 */
	uint32_t	uc_eip;			/* 0x0b8 */
	uint32_t	uc_seg_cs; 		/* 0x0bc */
	uint32_t	uc_eflags;		/* 0x0c0 */
	uint32_t	uc_esp;			/* 0x0c4 */
	uint32_t	uc_seg_ss;		/* 0x0c8 */
	unsigned char	uc_extended_regs[512]; /* 0x0cc */
} nt_thread_context_i386;
