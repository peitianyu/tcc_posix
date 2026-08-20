#ifndef _PSX_LDSO_H_
#define _PSX_LDSO_H_

#include "psx_systypes.h"

typedef void	(*__ctorfn_t)();
typedef void	(*__dtorfn_t)();

struct __psx_guid_str {
	char	group1[8];
	char	group2[4];
	char	group3[4];
	char	group4[4];
	char	group5[12];
};

struct __psx_dso_descriptor {
	void *	dso_main_routine;
	void *	dso_entry_point;
	void *	dtvptr;
	void *	tlsidx;
	void *	reserved[16];
};

struct __psx_common_descriptor {
	struct __psx_guid_str	guid;
	int32_t			ver_major;
	int32_t			ver_minor;
	int32_t			rev_major;
	int32_t			rev_minor;
	__ctorfn_t *		ctors;
	__dtorfn_t *		dtors;
	void *			reserved[16];
};

void __psx_do_global_ctors(void);
void __psx_do_global_dtors(void);

#endif
