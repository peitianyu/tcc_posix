#ifndef _PSX_MMAN_H_
#define _PSX_MMAN_H_

#include "psx_systypes.h"

#define MAP_FAILED	((void *) -1)

#define	PROT_NONE	0x00000000
#define	PROT_READ	0x00000001
#define	PROT_WRITE	0x00000002
#define	PROT_EXEC	0x00000004
#define PROT_GROWSDOWN	0x01000000
#define PROT_GROWSUP	0x02000000

#define	MAP_SHARED	0x01
#define	MAP_PRIVATE	0x02
#define	MAP_FIXED	0x10

#define MAP_FILE	0x00000000
#define MAP_TYPE	0x0000000f
#define MAP_ANON	0x00000020
#define MAP_ANONYMOUS	MAP_ANON
#define MAP_32BIT	0x00000040
#define MAP_GROWSDOWN	0x00000100
#define MAP_DENYWRITE	0x00000800
#define MAP_EXECUTABLE	0x00001000
#define MAP_LOCKED	0x00002000
#define MAP_NORESERVE	0x00004000
#define MAP_POPULATE	0x00008000
#define MAP_NONBLOCK	0x00010000
#define MAP_STACK	0x00020000
#define MAP_HUGETLB	0x00040000

struct __psx_tlca;
struct __psx_ctx;

struct __mmap_ctx {
	void *			addr;
	size_t			reserve;
	size_t			commit;
	struct __psx_tlca *	tlca;
	nt_oa			oa;
	uint32_t		fsection;
	uint32_t		cprot;
	uint32_t		mprot;
	uint32_t		fpage;
	uint32_t		attr;
	nt_section_inherit	share;
	struct __ofd *		ofd;
	void *			hfile;
	void *			hsection;
	nt_large_integer	size;
	nt_large_integer	foffset;
};

int32_t			__psx_section_add(struct __psx_ctx * ctx, struct __mmap_ctx * mapinfo);
struct __mmap_ctx *	__psx_section_get(struct __psx_ctx * ctx, void * base, void * any);

#endif
