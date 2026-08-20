/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_path.h"
#include "psx_unicode.h"
#include "psx.h"

int32_t __psx_path_get_name_info(struct __path_info * path_info)
{
	int32_t		status;
	nt_statfs *	nstatfs;
	uintptr_t *	statbuf;
	uintptr_t	bufaddr;
	size_t		bufsize;
	size_t		strlen;
	char *		ch;
	wchar16_t *	wch;

	/* validation */
	if (!path_info->fsbuf || ((uintptr_t)path_info->fsbuf % sizeof(size_t)))
		return NT_STATUS_INTERNAL_ERROR;
	else if (path_info->fsbuflen < __PSX_VIRTUAL_PAGE_SIZE)
		return NT_STATUS_BUFFER_TOO_SMALL;

	/* native statfs */
	nstatfs = (nt_statfs *)path_info->fsbuf;
	nstatfs->dev_name_maxlen = (uint16_t)(path_info->fsbuflen/2);

	bufaddr = (uintptr_t)nstatfs->dev_name + nstatfs->dev_name_maxlen;
	bufaddr += sizeof(uintptr_t) - 1;
	bufaddr |= sizeof(uintptr_t) - 1;
	bufaddr ^= sizeof(uintptr_t) - 1;

	bufsize = (uintptr_t)path_info->fsbuf + path_info->fsbuflen - bufaddr;
	statbuf = (uintptr_t *)bufaddr;

	if ((status = __ntapi->tt_statfs(
			path_info->hfile,0,0,
			nstatfs,statbuf,(uint32_t)bufsize,
			NT_STATFS_DOS_DRIVE_LETTER | NT_STATFS_DEV_NAME_COPY)))
		return status;

	nstatfs->dev_name_maxlen   = nstatfs->dev_name_strlen + nstatfs->record_name_strlen;

	path_info->fsname_utf16    = nstatfs->dev_name;
	path_info->fsnamelen_utf16 = nstatfs->dev_name_maxlen;

	wch = path_info->fsname_utf16 + nstatfs->dev_name_maxlen/sizeof(wchar16_t);
	*wch++ = 0;

	/* posix path name */
	path_info->fsname_utf8 = (char *)wch;
	bufsize = path_info->fsbuf + path_info->fsbuflen - path_info->fsname_utf8;
	ch = path_info->fsname_utf8;

	/* todo: root-relative presentation */
	if (path_info->pathflags & PSX_PATH_ROOT_RELATIVE)
		(void)0;

	if (nstatfs->nt_drive_letter) {
		*ch++ = '/';
		*ch++ = '/';

		if (nstatfs->nt_drive_letter < 'a')
			*ch++ = (char)nstatfs->nt_drive_letter + 'a' - 'A';
		else
			*ch++ = (char)nstatfs->nt_drive_letter;

		bufsize -= 3;
		strlen = nstatfs->record_name_strlen;
		wch = nstatfs->dev_name + nstatfs->dev_name_strlen/sizeof(wchar16_t);
	} else {
		strlen = nstatfs->dev_name_strlen + nstatfs->record_name_strlen;
		wch = (wchar16_t *)nstatfs->dev_name;
	}

	if ((status = __psx_strconv_utf16_to_utf8(0,wch,ch,strlen,bufsize,0)))
		return status;

	for (ch=path_info->fsname_utf8; *ch; ch++)
		if (*ch == '\\')
			*ch = '/';

	path_info->fsnamelen_utf8 = ch - path_info->fsname_utf8;

	return NT_STATUS_SUCCESS;
}
