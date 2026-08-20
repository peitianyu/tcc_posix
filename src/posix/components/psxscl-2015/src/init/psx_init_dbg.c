/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_debug.h"
#include "psx_impl.h"
#include "psx_init.h"
#include "psx_path.h"
#include "psx_cwd.h"
#include "psx_acl.h"
#include "psx.h"

/**
 * change image file name in the native PEB from a native
 * path to a DOS path so that execve'd programs can be
 * easily debugged by currently available debuggers. this
 * hack will be removed once lldb and gdb have been ported.
 *
 * [some users might argue that this is always a good idea
 *  due to select third-party process explorers getting
 *  confused by a native image file name in the PEB]
**/

int32_t __psx_init_dbg(void)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	nt_peb *		peb;
	wchar16_t *		wch;
	nt_unicode_string	name;
	nt_unicode_string *	image;
	nt_statfs *		statfs;
	uint16_t		strlen;

	if (!(rtdata->envc))
		return NT_STATUS_SUCCESS;

	tlca = __tlca_self();
	peb  = (nt_peb *)pe_get_peb_address();

	__ntapi->tt_init_unicode_string_from_utf16(
		&name,rtdata->peb_wargv[0]);

	statfs = (nt_statfs *)((uintptr_t)tlca->buffer + __PSX_VIRTUAL_PAGE_SIZE);
	statfs->dev_name_maxlen = (uint16_t)(tlca->buflen - __PSX_VIRTUAL_PAGE_SIZE);

	if ((status = __ntapi->tt_statfs(
			0,0,&name,
			statfs,
			tlca->buffer,
			__PSX_VIRTUAL_PAGE_SIZE,
			NT_STATFS_DOS_DRIVE_LETTER|NT_STATFS_DEV_NAME_COPY)))
		return status;

	if (!statfs->nt_drive_letter)
		return NT_STATUS_SUCCESS;

	image  = &peb->process_params->image_file_name;
	strlen = statfs->record_name_strlen + 6;

	if (image->strlen < strlen)
		return NT_STATUS_INTERNAL_ERROR;

	__ntapi->tt_generic_memset(
		image->buffer,0,image->strlen);

	image->strlen = strlen;
	wch = image->buffer;

	*wch++ = statfs->nt_drive_letter;
	*wch++ = ':';

	__ntapi->tt_memcpy_utf16(
		wch,
		statfs->dev_name + (statfs->dev_name_strlen / sizeof(wchar16_t)),
		statfs->record_name_strlen);





	return NT_STATUS_SUCCESS;
}
