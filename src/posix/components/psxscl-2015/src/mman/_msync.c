/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  tcc_posix: 2015 pre-alpha 未实现 msync (musl mmap   */
/*  文件映射的写回依赖). 用 NT 的 flush_virtual_memory  */
/*  (FlushViewOfFile 等价).                            */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_tlca.h"
#include "psx.h"

__psx_api
intptr_t __sys_msync(void * addr, size_t length, int flags)
{
	struct __psx_tlca *	tlca;
	int32_t			status;
	uintptr_t		region_size;

	(void)flags; /* MS_SYNC 仅同步语义 */

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	region_size = (uintptr_t)length;
	status = __ntapi->zw_flush_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&addr,
		&region_size,
		0);

	if (status)
		return __psx_sig_epilog(tlca,-EIO,status);

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
