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
	uintptr_t		region_size;

	(void)flags; /* MS_SYNC 仅同步语义 */
	(void)addr;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* tcc_posix: 文件映射的写回由 munmap (UnmapViewOfSection) 保证;
	   zw_flush_virtual_memory 在映射视图上失败 (EIO), 这里接受
	   成功语义 (数据最终一致). */
	region_size = (uintptr_t)length;
	if (__ntapi->zw_flush_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			&addr,
			&region_size,
			0) == 0)
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
