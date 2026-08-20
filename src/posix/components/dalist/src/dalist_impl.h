#include <dalist/dalist.h>

/* internal synonyms and prototypes */
typedef dalist_memfn_custom	memfn_custom;
typedef dalist_memfn_mmap	memfn_mmap;
typedef dalist_memfn_malloc	memfn_malloc;
typedef dalist_memfn_nt_allocvm	memfn_allocvm;


/* memfn_allocvm */
#define NT_STATUS_SUCCESS		0
#define NT_CURRENT_PROCESS_HANDLE	(void *)(uintptr_t)-1
#define NT_PAGE_READWRITE		(0x0004u)
#define NT_MEM_COMMIT			(0x1000u)
#define NT_MEM_RESERVE			(0x2000u)
#define NT_MEM_DECOMMIT			(0x4000u)
#define NT_MEM_RELEASE			(0x8000u)

/* host environment */
#if defined (_MIDIPIX_FREESTANDING)
#define dalist_errno(x) x
#define PROT_READ	1
#define PROT_WRITE	2
#define MAP_ANON	0x20
#define MAP_SHARED	0x01
#else
#define dalist_errno(x) errno
#endif

struct dalist_iosb {
	union {
		int32_t		status;
		void *		pointer;
	};
	intptr_t		info;
};


typedef int dalist_dbg_write(
	struct dalist_ex *	dlist,
	intptr_t		fildes_or_hfile,
	const void *		buf,
	size_t			nbyte);
