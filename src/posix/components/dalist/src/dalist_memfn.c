/*****************************************************************************/
/*  dalist: a zero-dependency book-keeping library                           */
/*  Copyright (C) 2013--2021  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.DALIST.                      */
/*****************************************************************************/

#include <dalist/dalist.h>
#include "dalist_impl.h"

/* private prototypes */
static void * dalist_alloc(struct dalist_ex * dlist);

static int dalist_memfn_internal(
	struct dalist_ex *	dlist,
	void **			addr,
	size_t *		alloc_size);


dalist_api
int dalist_get_free_node(
	struct dalist_ex *	dlist,
	void **			fnode)
{
	int			ret;
	struct dalist_node *	nfree;

	/* in case we fail */
	*fnode = (void *)0;

	if (dlist->free)
		/* allocation is not needed */
		nfree  = (struct dalist_node *)dlist->free;
	else
		nfree = (struct dalist_node *)dalist_alloc(dlist);

	if (nfree) {
		*fnode = nfree;
		dlist->free = nfree->next;

		if (nfree->next) /* new head of dlist->free */
			nfree->next->prev = 0;

		/* bookkeeping */
		dlist->info.free_nodes--;

		/* and for good measure */
		nfree->prev = nfree->next = 0;
		ret = DALIST_OK;
	} else
		ret = DALIST_EMEMFN;

	return ret;
}


dalist_api
int dalist_deposit_free_node(
	struct dalist_ex *	dlist,
	void *			fnode)
{
	int			ret;
	struct dalist_node *	nfree;

	if (fnode) {
		nfree       = (struct dalist_node *)fnode;
		nfree->next = (struct dalist_node *)dlist->free;

		/* fnode is now the head free node */
		nfree->prev = 0;
		dlist->free = fnode;

		/* bookkeeping */
		dlist->info.free_nodes++;
		ret = DALIST_OK;
	} else {
		ret = DALIST_ENODE;
	}

	return ret;
}


dalist_api
int dalist_deposit_memory_block(
	struct dalist_ex *	dlist,
	void *			addr,
	size_t			alloc_size)
{
	struct dalist_node *	fnode;
	struct dalist_node *	fnode_next;
	char *			naddr;
	char *			naddr_next;
	char *			naddr_upper;
	size_t			node_size;

	/* node_size: before alignment */
	if (dlist->dblock_size == 0)
		node_size = sizeof(struct dalist_node);
	else
		node_size = (size_t)&((struct dalist_node_ex *)0)->dblock
				+ dlist->dblock_size;

	/* node_size: after alignment */
	node_size += sizeof(uintptr_t)-1;
	node_size |= sizeof(uintptr_t)-1;
	node_size ^= sizeof(uintptr_t)-1;

	/* validate block size */
	if (alloc_size < node_size)
		return DALIST_EMEMBLOCK;

	/* marks */
	naddr       = (char *)addr;
	naddr_next  = naddr + node_size;
	naddr_upper = naddr + alloc_size;

	/* chain of free nodes */
	while ((naddr_next + node_size) <= naddr_upper) {
		fnode      = (struct dalist_node *)naddr;
		fnode_next = (struct dalist_node *)naddr_next;

		fnode->next      = fnode_next;
		fnode_next->prev = fnode;

		naddr      += node_size;
		naddr_next += node_size;
	}

	/* free nodes block: head and tail */
	fnode       = (struct dalist_node *)addr;
	fnode_next  = (struct dalist_node *)naddr;

	if (dlist->free) {
		/* link with pre-allocated free nodes */
		fnode_next->next = (struct dalist_node *)dlist->free;
		((struct dalist_node *)dlist->free)->prev = fnode_next;
	} else {
		/* no pre-allocated free nodes */
		fnode_next->next = 0;
	}

	/* fnode is now the head free node */
	fnode->prev = 0;
	dlist->free = fnode;

	/* bookkeeping */
	dlist->info.free_nodes += (alloc_size / node_size);

	return DALIST_OK;
}


static void * __cdecl dalist_alloc(struct dalist_ex * dlist)
{
	int			ret;
	void *			addr;
	size_t			alloc_size;
	memfn_custom *		pmemfn;
	struct dalist_node *	fnode;

	/* pmemfn */
	if (dlist->memfn_method == DALIST_MEMFN_CUSTOM)
		pmemfn = (dalist_memfn_custom *)(uintptr_t)dlist->memfn_ptr;
	else
		pmemfn = dalist_memfn_internal;

	/* allocate: try */
	addr  = 0;
	fnode = 0;
	ret   = pmemfn(dlist,&addr,&alloc_size);

	/* allocate: verify */
	if (ret == DALIST_OK) {
		if (dlist->memfn_method == DALIST_MEMFN_MALLOC)
			fnode = (struct dalist_node *)addr;
		else if (dalist_deposit_memory_block(
				dlist,
				addr,
				alloc_size) == DALIST_OK)
			fnode = (struct dalist_node *)dlist->free;
	}

	return fnode;
}


static int __cdecl dalist_memfn_internal(
	struct dalist_ex *	dlist,
	void **			address,
	size_t *		alloc_size)
{
	int		ret;
	void *		addr;
	size_t		size;

	memfn_mmap *	pfn_mmap;
	memfn_malloc *	pfn_malloc;
	memfn_allocvm * pfn_allocvm;

	switch (dlist->memfn_method) {
		case DALIST_MEMFN_MALLOC:
			if (dlist->dblock_size == 0)
				size = sizeof(struct dalist_node);
			else
				size = (size_t)((struct dalist_node_ex *)0)->dblock
					+ dlist->dblock_size;

			pfn_malloc = (memfn_malloc *)(uintptr_t)dlist->memfn_ptr;
			addr = pfn_malloc(size);

			if (addr) {
				*address = addr;
				*alloc_size = size;
				ret = dlist->memfn_status = DALIST_OK;
			} else {
				ret = DALIST_EMEMFN;
				dlist->memfn_status = dalist_errno(ret);
			}
			break;

		case DALIST_MEMFN_MMAP:
			pfn_mmap = (memfn_mmap * )(uintptr_t)dlist->memfn_ptr;

			addr = pfn_mmap(
				(void *)0,
				dlist->lblock_size,
				PROT_READ | PROT_WRITE,
				MAP_ANON | MAP_SHARED,
				0,0);

			if (addr) {
				*address = addr;
				*alloc_size = dlist->lblock_size;
				ret = dlist->memfn_status = DALIST_OK;
			} else {
				ret = DALIST_EMEMFN;
				dlist->memfn_status = dalist_errno(ret);
			}
			break;

		case DALIST_MEMFN_NT_ALLOCATE_VIRTUAL_MEMORY:
			addr       = (void *)0;
			size = dlist->lblock_size;
			pfn_allocvm = (memfn_allocvm *)(uintptr_t)dlist->memfn_ptr;

			dlist->memfn_status = pfn_allocvm(
				NT_CURRENT_PROCESS_HANDLE,
				&addr,
				0,
				&size,
				NT_MEM_COMMIT,
				NT_PAGE_READWRITE);

			if (dlist->memfn_status == NT_STATUS_SUCCESS) {
				*address = addr;
				*alloc_size = size;
				ret = DALIST_OK;
			} else {
				ret = DALIST_EMEMFN;
			}
			break;
		default:
			ret = DALIST_EINTERNAL;
			break;
	}

	return ret;
}
