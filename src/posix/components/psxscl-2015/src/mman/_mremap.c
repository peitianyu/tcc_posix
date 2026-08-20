/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_ctx.h"
#include "psx_errno.h"
#include "psx_flags.h"
#include "psx_mman.h"
#include "psx_signal.h"
#include "psx_ofd.h"
#include "psx_acl.h"
#include "psx.h"



__psx_api
void * __sys_mremap(void * mapaddr, size_t mapsize, size_t newsize, int flags)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __mmap_ctx *	m;
	struct __mmap_ctx	ext;

	tlca = __tlca_self();
	if (!(ctx = __tlca_shared_ctx(tlca))) return 0;

	if (!(m = __psx_section_get(ctx,0,mapaddr)))
		return (void *)__psx_sig_epilog(tlca,-EINVAL,EPSXONLY);


	/* lottery */
	if (m->commit >= newsize)
		return (void *)__psx_sig_epilog(tlca,(intptr_t)m->addr,NT_STATUS_SUCCESS);

	else if (m->hsection && !(status = __ntapi->zw_extend_section(m->hsection,&m->size)))
		return (void *)__psx_sig_epilog(tlca,(intptr_t)m->addr,NT_STATUS_SUCCESS);



	/* common */
	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&ext,
		(uintptr_t *)m,
		sizeof(ext));

	ext.tlca	= tlca;
	ext.addr	= (void *)((uintptr_t)m->addr + m->commit);
	ext.commit	= newsize - m->commit;



	/* easy way out */
	if (!m->hfile && newsize <= (size_t)m->size.quad) {
		if ((status = __ntapi->zw_allocate_virtual_memory(
				NT_CURRENT_PROCESS_HANDLE,
				&ext.addr,0,
				&ext.commit,
				NT_MEM_COMMIT,
				ext.mprot)))
			return (void *)__psx_sig_epilog(ext.tlca,-ENOMEM,status);

		m->commit += ext.commit;

		return (void *)__psx_sig_epilog(tlca,(intptr_t)m->addr,NT_STATUS_SUCCESS);
	}







	/* ALL CODE BELOW NEEDS BOTH FIXING AND TESTING */









	/* tedious way out (todo: MREMAP_MAYMOVE) */
	ext.size.quad = newsize - m->size.quad;
	ext.foffset.quad = m->hfile
			? m->commit + m->foffset.quad
			: 0;

	if ((status = __ntapi->zw_create_section(
			&ext.hsection,
			ext.fsection | NT_SECTION_QUERY | NT_SECTION_EXTEND_SIZE,
			&ext.oa,&ext.size,ext.cprot,ext.attr,ext.hfile)))
		return (void *)__psx_sig_epilog(ext.tlca,-EFAULT,status);

	if ((status = __ntapi->zw_map_view_of_section(
			ext.hsection,
			NT_CURRENT_PROCESS_HANDLE,
			&ext.addr,0,ext.commit,&ext.foffset,&ext.commit,
			ext.share,0,ext.mprot)))
		return (void *)__psx_sig_epilog(ext.tlca,-ENOMEM,status);

	if ((status = __psx_section_add(ctx,&ext)))
		return (void *)__psx_sig_epilog(ext.tlca,-ENOMEM,status);

	return (void *)__psx_sig_epilog(tlca,(intptr_t)m->addr,NT_STATUS_SUCCESS);
}
