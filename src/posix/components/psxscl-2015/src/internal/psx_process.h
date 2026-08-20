/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_PROCESS_H_
#define _PSX_PROCESS_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_ofd.h"
#include "psx_tlca.h"

enum __psx_rtdata_user_pointers {
	PSX_RTDATA_UPTR_SECTION_FD,
	PSX_RTDATA_UPTR_SECTION_FD_BITMAP,
	PSX_RTDATA_UPTR_SECTION_OFD,
	PSX_RTDATA_UPTR_SECTION_OFD_BITMAP,
	PSX_RTDATA_UPTR_CAP
};

enum __psx_rtdata_udat32 {
	PSX_RTDATA_UDAT32_FD_CAP,
	PSX_RTDATA_UDAT32_OFD_CAP,
	PSX_RTDATA_UDAT32_CAP
};

#define PSX_CREATE_PROCESS_PID_TRANSFER		0x00000001
#define PSX_CREATE_PROCESS_CLONE_FD_TABLES	0x00000002

typedef int32_t __psx_create_process(
	struct __psx_tlca *	tlca,
	struct __ofd *		image,
	struct __ofd *		interpreter,
	const unsigned char *	optarg,
	const unsigned char *	path,
	const char **		argv,
	const char **		envp,
	uint32_t 		flags,
	const nt_alt_cid *	altcid,
	nt_process_info *	execve);

__psx_create_process	__psx_create_native_process;
__psx_create_process	__psx_create_foreign_process;

#endif
