/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include "psx_systypes.h"
#include "psx_impl.h"
#include "psx_unicode.h"
#include "psx_tlca.h"
#include "psx.h"

int32_t __psx_getcwd(struct __psx_ctx * ctx, char * buf, size_t size)
{
	/* todo: locking */
	if (--size < ctx->cwd.fsnamelen_utf8)
		return NT_STATUS_BUFFER_TOO_SMALL;

	__ntapi->tt_generic_memcpy(
		buf,
		ctx->cwd.fsname_utf8,
		ctx->cwd.fsnamelen_utf8 + 1);

	return NT_STATUS_SUCCESS;
}

int32_t __psx_setcwd(struct __psx_ctx * ctx, struct __path_info * path_info)
{
	/* todo: locking */
	if (path_info != &ctx->cwd)
		__ntapi->tt_aligned_block_memcpy(
			(uintptr_t *)&ctx->cwd,
			(uintptr_t *)path_info,
			(size_t)&(((struct __path_info *)0)->fsbuf));

	return __psx_path_get_name_info(&ctx->cwd);
}
