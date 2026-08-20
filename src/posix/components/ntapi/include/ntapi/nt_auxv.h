#ifndef _NT_AUXV_H_
#define _NT_AUXV_H_

#include <psxtypes/psxtypes.h>

#define NT_AT_NULL		0
#define NT_AT_IGNORE		1
#define NT_AT_EXECFD		2
#define NT_AT_PHDR		3
#define NT_AT_PHENT		4
#define NT_AT_PHNUM		5
#define NT_AT_PAGESZ		6
#define NT_AT_BASE		7
#define NT_AT_FLAGS		8
#define NT_AT_ENTRY		9
#define NT_AT_NOTELF		10
#define NT_AT_UID		11
#define NT_AT_EUID		12
#define NT_AT_GID		13
#define NT_AT_EGID		14
#define NT_AT_CLKTCK		17
#define NT_AT_PLATFORM		15
#define NT_AT_HWCAP		16
#define NT_AT_FPUCW		18
#define NT_AT_DCACHEBSIZE	19
#define NT_AT_ICACHEBSIZE	20
#define NT_AT_UCACHEBSIZE	21
#define NT_AT_IGNOREPPC		22
#define	AT_SECURE		23
#define NT_AT_BASE_PLATFORM 	24
#define NT_AT_RANDOM		25
#define NT_AT_HWCAP2		26
#define NT_AT_EXECFN		31
#define NT_AT_SYSINFO		32
#define NT_AT_SYSINFO_EHDR	33
#define NT_AT_L1I_CACHESHAPE	34
#define NT_AT_L1D_CACHESHAPE	35
#define NT_AT_L2_CACHESHAPE	36
#define NT_AT_L3_CACHESHAPE	37
#define NT_AT_ARRAY_CAP		38

typedef struct _nt_auxv_t {
	uintptr_t	a_type;

	union {
		uintptr_t a_val;
	} a_un;
} nt_auxv_t;

#endif
