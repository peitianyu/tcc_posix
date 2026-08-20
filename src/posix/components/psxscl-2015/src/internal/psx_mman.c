/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <dalist/dalist.h>
#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_mman.h"
#include "psx_debug.h"
#include "psx.h"

int32_t __psx_section_add(struct __psx_ctx * ctx, struct __mmap_ctx * mapinfo)
{
	int32_t			status;
	struct dalist_node_ex *	node;

	if ((status = dalist_get_node_by_key(
			&ctx->sections,&node,
			(uintptr_t)mapinfo->addr,
			DALIST_NODE_TYPE_NEW,0)))
		return status;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&node->dblock,
		(uintptr_t *)mapinfo,
		sizeof(*mapinfo));

	return status;
}


struct __mmap_ctx * __psx_section_get(struct __psx_ctx * ctx, void * base, void * any)
{
	int32_t			status;
	struct dalist_node_ex *	node;

	/* todo: add support for any */
	base = base ? base : any;

	if ((status = dalist_get_node_by_key(
			&ctx->sections,&node,
			(uintptr_t)base,
			DALIST_NODE_TYPE_EXISTING,0)))
		return 0;

	return node ? (struct __mmap_ctx *)&node->dblock : 0;
}
