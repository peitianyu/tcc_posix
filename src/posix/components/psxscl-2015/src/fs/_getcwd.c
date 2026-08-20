/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_cwd.h"
#include "psx_tlca.h"
#include "psx.h"

__psx_api
intptr_t __sys_getcwd(char * buf, size_t size)
{
	struct __psx_tlca *	tlca;
	int32_t			status;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	status = __psx_getcwd(tlca->ctx,buf,size);

	if (size == 0)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	else if (status == NT_STATUS_BUFFER_TOO_SMALL)
		return __psx_sig_epilog(tlca,-ERANGE,EPSXONLY);
	else
		return __psx_sig_epilog(tlca,0,status);
}
