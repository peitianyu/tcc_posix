/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_impl.h"
#include "psx_init.h"
#include "psx_path.h"
#include "psx_cwd.h"
#include "psx_acl.h"
#include "psx.h"

/* (-mposix only, otherwise irrelevant) */
#ifndef __PSX_DEFAULT_ROOT_DIRECTORY
#define __PSX_DEFAULT_ROOT_DIRECTORY { \
				'\\','?','?','\\', \
				'C',':','\\','m','i','d','i','p','i','x'}
#endif

int32_t __psx_init_cwd(void)
{
	int			status;
	nt_peb *		peb;
	wchar16_t *		drive_letter;
	void * hfile;

	nt_ohio			ohio;
	nt_unicode_string	root_path;
	nt_iosb			iosb;
	nt_oa			oa;
	const wchar16_t		root_path_name[] = __PSX_DEFAULT_ROOT_DIRECTORY;

status=0;
while (status);

	/* runtime-data present? */
	if (rtdata->hcwd) {
		rtctx.cwd.hfile  = rtdata->hcwd;
		rtctx.cwd.hat    = rtdata->hdrive;
	} else {
		peb		= (nt_peb *)(pe_get_peb_address());
		rtctx.cwd.hfile	= peb->process_params->cwd_handle;
		drive_letter	= peb->process_params->cwd_name.buffer;

		ohio.inherit		= 1;
		ohio.protect_from_close = 0;

		if ((status = __ntapi->zw_set_information_object(
				rtctx.cwd.hfile,
				NT_OBJECT_HANDLE_INFORMATION,
				&ohio,sizeof(ohio))))
			return status;

		if (drive_letter && *drive_letter && ((*(drive_letter + 1)) == ':'))
			if (!(__psx_get_dos_drive_root_handle(
					&hfile,
					(unsigned char *)drive_letter)))
				rtctx.cwd.hat = hfile;
	}

	if (rtdata->hroot)
		rtctx.root.hfile = rtdata->hroot;
	else if (1 || (rtdata->ctx_options & __PSXOPT_POSIX)) {
		/* -mposix: optional root directory */
		root_path.buffer = (uint16_t *)root_path_name;
		root_path.strlen = sizeof(root_path_name);
		root_path.maxlen = 0;

		oa.len		= sizeof(oa);
		oa.root_dir	= 0;
		oa.obj_name	= &root_path;
		oa.obj_attr	= NT_OBJ_INHERIT;
		oa.sec_desc	= __PSX_DEF_SEC_DESC;
		oa.sec_qos	= __PSX_DEF_SEC_QOS;

		status = __ntapi->zw_open_file(
			&hfile,
			NT_FILE_READ_ACCESS | NT_FILE_READ_ATTRIBUTES | NT_FILE_READ_EA,
			&oa,
			&iosb,
			NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
			NT_FILE_DIRECTORY_FILE | NT_FILE_OPEN_REPARSE_POINT);

		if (status == NT_STATUS_SUCCESS)
			rtctx.root.hfile = hfile;
	}

	rtctx.cwd.fsbuflen = __PSX_VIRTUAL_PAGE_SIZE;

	if ((status = __ntapi->zw_allocate_virtual_memory(
			rtdata->hprocess_self,
			(void **)&rtctx.cwd.fsbuf,
			0,&rtctx.cwd.fsbuflen,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE)))
		return status;

	rtctx.cwd.fdtype  = PSX_FD_OS_FS_DIR;
	rtctx.root.fdtype = PSX_FD_OS_FS_DIR;

	return __psx_setcwd(&rtctx,&rtctx.cwd);
}
