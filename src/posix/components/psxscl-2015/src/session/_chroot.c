/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include "psx_systypes.h"
#include "psx_session.h"
#include "psx_tlca.h"
#include "psx_session.h"
#include "psx_fcntl.h"
#include "psx_helper.h"
#include "psx_errno.h"
#include "psx_debug.h"
#include "psx.h"

__psx_api
intptr_t __sys_chroot(const unsigned char * path)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __path_info	path_info;
	struct __path_info	prev_root;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((status = __psx_path_open(tlca,&path_info,path,O_DIRECTORY,0,0,0,PSX_PATH_ACCESS_CHECK|PSX_PATH_LIST_DIR)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&prev_root,
		(uintptr_t *)&tlca->ctx->root,
		sizeof(path_info));

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&tlca->ctx->root,
		(uintptr_t *)&path_info,
		sizeof(path_info));

	if (prev_root.ofd)
		__psx_ofd_free(tlca->ctx,prev_root.ofd);

	else
		__ntapi->zw_close(prev_root.hfile);

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
