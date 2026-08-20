#ifndef DALIST_H
#define DALIST_H

#include "dalist_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* dalist node types */
enum dalist_node_types {
	DALIST_NODE,
	DALIST_NODE_EX,
};


/* dalist caller-provided memory allocation methods */
enum dalist_memfn {
	DALIST_MEMFN_CUSTOM,
	DALIST_MEMFN_MMAP,
	DALIST_MEMFN_MALLOC,
	DALIST_MEMFN_NT_ALLOCATE_VIRTUAL_MEMORY,
};


/* dalist debug environments */
enum dalist_dbgenv {
	DALIST_DEBUG_ENV_POSIX,
	DALIST_DEBUG_ENV_NT,
};


/* dalist return values */
#define DALIST_OK			(0x0)
#define DALIST_EINTERNAL		(0xD0000000)	/* internal error */
#define DALIST_EMEMFN			(0xD0000001)	/* memory allocation error */
#define DALIST_EMEMBLOCK		(0xD0000002)	/* invalid memory block */
#define DALIST_ELIST			(0xD0000003)	/* invalid list */
#define DALIST_ENODE			(0xD0000004)	/* invalid node */
#define DALIST_EPARAMMIX		(0xD0000005)	/* invalid parameter mix */
#define DALIST_EKEYEXISTS		(0xD0000006)	/* key already exists */
#define DALIST_ELISTNOTEMPTY		(0xD0000007)	/* null insertion point with non-empty list */

/* dalist debug return values */
#define DALIST_EDEBUGSTRUCT		(0xD0001000)	/* invalid debug structure */
#define DALIST_EDEBUGENV		(0xD0001001)	/* invalid debug environment */
#define DALIST_EDEBUGSPRINTF		(0xD0001002)	/* invalid debug pointer to sprintf */
#define DALIST_EDEBUGWRITE		(0xD0001003)	/* invalid debug pointer to write (posix) */
#define DALIST_EDEBUGWRITEFILE		(0xD0001004)	/* invalid debug pointer to zw_write_file (nt) */


/* dalist node flag bits */
#define DALIST_NODE_TYPE_NONE		(0x00u)
#define DALIST_NODE_TYPE_EXISTING	(0x01u)
#define DALIST_NODE_TYPE_NEW		(0x02u)
#define DALIST_NODE_TYPE_DELETED	(0x04u)
#define DALIST_NODE_MEM_BLOCK_BASE	(0x80u)


struct dalist_node {
	struct dalist_node *	prev;
	struct dalist_node *	next;
	uintptr_t		any;
};


struct dalist {
	struct dalist_node *	head;
	struct dalist_node *	tail;
};


struct dalist_node_ex {
	struct dalist_node_ex *	prev;
	struct dalist_node_ex *	next;
	uintptr_t		key;
	uintptr_t		flags;
	uintptr_t		dblock;
};


struct dalist_debug;

struct dalist_ex {
	void *			head;		/************************************/
	void *			tail;		/* head, tail, free:                */
	void *			free;		/* dalist_node when dblock_size is  */
	uintptr_t *		bookmarks;	/* zero, dalist_node_ex otherwise.  */
	size_t			dblock_size;	/*                                  */
	size_t			lblock_size;	/* lblock_size:                     */
	void *			memfn_ptr;	/* used when memfn_method is mmap   */
	enum dalist_memfn	memfn_method;	/* or nt_allocate_virtual_memory    */
	int			memfn_status;	/* as the system call's alloc_size  */
	int			mmap_prot;	/* argument.                        */
	int			mmap_flags;	/*                                  */
	int			mmap_fildes;	/*                                  */
	intptr_t		mmap_offset;	/************************************/
	struct dalist_debug *	debug;

	struct dalist_info {
		uintptr_t	list_nodes;
		uintptr_t	free_nodes;
		uintptr_t	bookmark_from;
		uintptr_t	bookmark_to;
		uintptr_t	bookmark_key_min;
		uintptr_t	bookmark_key_max;
		uintptr_t	nodes_below_first_bookmark;
		uintptr_t	nodes_above_last_bookmark;
	} info;
};


/* signatures for caller-provided debug output functions */
typedef int	__cdecl	  dalist_sprintf(char * buffer, const char * format, ...);
typedef ssize_t	__cdecl	  dalist_write(int fildes, const void *buf, size_t nbyte);
typedef int32_t __stdcall dalist_write_file(
	void *		hfile,
	void *		hevent,
	void *		apc_routine,
	void * 		apc_context,
	void * 		io_status_block,
	void * 		buffer,
	uint32_t	bytes_sent,
	int64_t *	byte_offset,
	uint32_t *	key);


/* debug */
struct dalist_debug {
	enum dalist_dbgenv	dbgenv;
	dalist_sprintf *	sprintf;	/* required (all) */
	dalist_write *		write;		/* required (posix only) */
	dalist_write_file *	zw_write_file;	/* required (nt only) */
};


/* signatures for caller-provided memory allocation functions */
typedef void * __cdecl dalist_memfn_mmap(
	void *		addr,
	size_t		alloc_size,
	int		prot,
	int		flags,
	int		fildes,
	intptr_t	offset);


typedef void * __cdecl dalist_memfn_malloc(
	size_t		alloc_size);


typedef int32_t __stdcall dalist_memfn_nt_allocvm(
	void *		hprocess,
	void **		base_address,
	uint32_t	zero_bits,
	size_t *	alloc_size,
	uint32_t	alloc_type,
	uint32_t	protect);


/**
 *  dalist_memfn_custom:
 *  must return either DALIST_OK or DALIST_EMEMFN;
 *  may update the list's memfn_status member
**/
typedef int __cdecl dalist_memfn_custom(
	struct dalist_ex *	dlist,
	void **			addr,
	size_t *		alloc_size);

dalist_api
int dalist_init(struct dalist *	dlist);

dalist_api
int dalist_init_ex(
	struct dalist_ex *	dlist,
	size_t			dblock_size,
	size_t			lblock_size,
	void *			memfn_ptr,
	enum dalist_memfn	memfn_method);

dalist_api
int dalist_insert_before(
	struct dalist *		dlist,
	struct dalist_node *	lnode,
	struct dalist_node *	nnode);

dalist_api
int dalist_insert_after(
	struct dalist *		dlist,
	struct dalist_node *	lnode,
	struct dalist_node *	nnode);

dalist_api
int dalist_get_free_node(
	struct dalist_ex *	dlist,
	void **			fnode);

dalist_api
int dalist_deposit_free_node(
	struct dalist_ex *	dlist,
	void *			fnode);

dalist_api
int dalist_deposit_memory_block(
	struct dalist_ex *	dlist,
	void *			addr,
	size_t			alloc_size);

dalist_api
int dalist_unlink_node(
	struct dalist *		dlist,
	struct dalist_node *	node);

dalist_api
int dalist_unlink_node_ex(
	struct dalist_ex *	dlist,
	struct dalist_node_ex *	node);

dalist_api
int dalist_discard_node(
	struct dalist_ex *	dlist,
	struct dalist_node_ex *	node);

dalist_api
int dalist_get_node_by_key(
	struct dalist_ex *	dlist,
	struct dalist_node_ex **node,
	uintptr_t		key,
	uintptr_t		get_flags,
	uintptr_t *		node_flags);

dalist_api
int dalist_insert_node_by_key(
	struct dalist_ex *	dlist,
	struct dalist_node_ex *	node);

/* debug routines */
dalist_api
int dalist_debug_setup(
	struct dalist_ex *	dlist,
	struct dalist_debug *	dlist_debug,
	enum dalist_dbgenv	dbgenv,
	dalist_sprintf *	pfn_sprintf,
	dalist_write *		pfn_write,
	dalist_write_file *	pfn_write_file);

dalist_api
int dalist_debug_print(
	struct dalist_ex *	dlist,
	intptr_t		fildes_or_hfile);


#ifdef __cplusplus
}
#endif
#endif
