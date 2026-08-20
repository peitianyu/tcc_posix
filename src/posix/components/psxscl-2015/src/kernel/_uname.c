/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_sysinfo.h"
#include "psx.h"

#if defined(__X86_MODEL)
#define __MACHINE_NAME "i686"
#elif defined(__X86_64_MODEL)
#define __MACHINE_NAME "x86_64"
#endif

static struct __utsname __utsname = {
	"midipix",
	"node",
	"pre-pre-alpha-experimental-tree-internal-testing-only",
	"0.0.0.0.0.0.0.1",
	__MACHINE_NAME
};

__psx_api
intptr_t __sys_uname(struct __utsname * utsname)
{
	struct __psx_tlca *	tlca;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	__ntapi->tt_generic_memcpy(
		(char *)utsname,
		(char *)&__utsname,
		sizeof(*utsname));

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
