/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "pe_os.h"

static int32_t pe_get_device_dos_drive_letter(
	void *			hntdll,
	os_zw_query_object *	zw_query_object,
	uintptr_t *		dbuffer,
	uint32_t		dbufsize,
	wchar16_t *		devname,
	size_t			devnamelen,
	wchar16_t *		letter)
{
	int32_t			status;
	void *			hdevice;
	int			sigh;
	size_t			idx;
	size_t			cap;
	uint32_t		len;
	struct os_oa		oa;
	struct os_iosb		iosb;
	wchar16_t *		src;
	wchar16_t *		dst;
	struct pe_unicode_str	path;
	struct pe_unicode_str * dpath;
	os_zw_open_file *	zw_open_file;
	os_zw_close *		zw_close;

	wchar16_t		namebuf[8] = {
				'\\','?','?','\\',
				'X',':',0};

	unsigned char		letters[26] = {
				'C','Z','Y','X','W','V',
				'E','H','F','G','D','I',
				'P','Q','R','S','T','U',
				'J','K','L','M','N','O',
				'A','B'};

	/* init */
	if (!(zw_open_file = (os_zw_open_file *)pe_get_procedure_address(
			hntdll,"ZwOpenFile")))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_close = (os_zw_close *)pe_get_procedure_address(
			hntdll,"ZwClose")))
		return OS_STATUS_INTERNAL_ERROR;

	/* path */
	path.buffer = namebuf;
	path.strlen = 6 * sizeof(wchar16_t);
	path.maxlen = 0;

	/* oa */
	oa.len      = sizeof(struct os_oa);
	oa.root_dir = 0;
	oa.obj_name = &path;
	oa.obj_attr = 0;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* just a few rounds of submissiveness */
	for (sigh=0; sigh<26; sigh++) {
		namebuf[4] = letters[sigh];

		status = zw_open_file(
			&hdevice,
			OS_SEC_SYNCHRONIZE
				| OS_FILE_READ_ATTRIBUTES,
			&oa,&iosb,
			OS_FILE_SHARE_READ
				| OS_FILE_SHARE_WRITE
				| OS_FILE_SHARE_DELETE,
			OS_FILE_SYNCHRONOUS_IO_ALERT);

		if (status == OS_STATUS_SUCCESS) {
			status = zw_query_object(
				hdevice,
				OS_OBJECT_NAME_INFORMATION,
				dbuffer,dbufsize,&len);

			if (status == OS_STATUS_SUCCESS) {
				dpath = (struct pe_unicode_str *)dbuffer;

				src = devname;
				dst = dpath->buffer;

				if (devnamelen == dpath->strlen) {
					idx = 0;
					cap = devnamelen / sizeof(wchar16_t);

					for (; idx<cap && src[idx]==dst[idx]; )
						idx++;

					if (idx==cap) {
						zw_close(hdevice);
						*letter = letters[sigh];

						return OS_STATUS_SUCCESS;
					}
				}
			}

			zw_close(hdevice);
		}
	}

	return OS_STATUS_NOT_SUPPORTED;
}


static int32_t pe_load_library_impl(
	void **		baseaddr,
	void *		hdsolib,
	uintptr_t *	buffer,
	uint32_t	bufsize,
	uint32_t *	flags)
{
	int32_t			status;
	struct pe_unicode_str	path;
	struct pe_unicode_str *	npath;
	uintptr_t		addr;
	uintptr_t *		dbuffer;
	uint32_t		dbufsize;
	wchar16_t *		ldrdir;
	wchar16_t *		slash;
	wchar16_t *		wch;
	wchar16_t *		cap;
	wchar16_t		letter;
	uint32_t		len;
	int			mup;
	void *			hntdll;
	os_zw_query_object *	zw_query_object;
	os_ldr_load_dll *	ldr_load_dll;

	/* init */
	if (!(hntdll = pe_get_ntdll_module_handle()))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_query_object = (os_zw_query_object *)pe_get_procedure_address(
			hntdll,"ZwQueryObject")))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(ldr_load_dll = (os_ldr_load_dll *)pe_get_procedure_address(
			hntdll,"LdrLoadDll")))
		return OS_STATUS_INTERNAL_ERROR;

	/* loader native path */
	if ((status = zw_query_object(
			hdsolib,
			OS_OBJECT_NAME_INFORMATION,
			buffer,bufsize,&len)))
		return status;

	/* integrity */
	if (len == sizeof(struct pe_unicode_str))
		return OS_STATUS_BAD_FILE_TYPE;

	/* first sigh */
	npath = (struct pe_unicode_str *)buffer;

	wch = npath->buffer;
	cap = npath->buffer + (npath->strlen / sizeof(wchar16_t));

	if ((cap < &wch[8])
			|| (wch[0] != '\\')
			|| (wch[1] != 'D')
			|| (wch[2] != 'e')
			|| (wch[3] != 'v')
			|| (wch[4] != 'i')
			|| (wch[5] != 'c')
			|| (wch[6] != 'e')
			|| (wch[7] != '\\'))
		return OS_STATUS_NOT_SUPPORTED;

	mup = (cap > &wch[11])
		&& (wch[8]=='M')
		&& (wch[9]=='u')
		&& (wch[10]=='p')
		&& (wch[11]=='\\');

	slash = mup ? &wch[12] : &wch[8];

	for (; (*slash != '\\') && (slash < cap); )
		slash++;

	if (slash == cap)
		return OS_STATUS_INTERNAL_ERROR;

	if (mup)
		for (++slash; (*slash != '\\') && (slash < cap); )
			slash++;

	if (slash == cap)
		return OS_STATUS_INTERNAL_ERROR;

	/* second sigh */
	addr  = (uintptr_t)cap;
	addr += sizeof(uintptr_t) - 1;
	addr /= sizeof(uintptr_t);
	addr *= sizeof(uintptr_t);

	dbuffer   = (uintptr_t *)addr;
	dbufsize  = bufsize;
	dbufsize -= (dbuffer - buffer) * sizeof(uintptr_t);

	if ((status = pe_get_device_dos_drive_letter(
			hntdll,
			zw_query_object,
			dbuffer,dbufsize,
			npath->buffer,
			(slash - npath->buffer) * sizeof(wchar16_t),
			&letter)))
		return status;

	slash    = &slash[-2];
	slash[0] = letter;
	slash[1] = ':';

	path.buffer = slash;
	path.strlen = (cap - slash) * sizeof(wchar16_t);

	/* third sigh */
	for (slash=cap; (slash > path.buffer) && (*slash != '\\'); )
		slash--;

	if (slash == path.buffer)
		return OS_STATUS_INTERNAL_ERROR;

	ldrdir = path.buffer;
	*slash = 0;

	path.strlen -= (++slash - ldrdir) * sizeof(wchar16_t);
	path.maxlen  = path.strlen;
	path.buffer  = slash;

	/* hoppla */
	return ldr_load_dll(
		ldrdir,flags,
		&path,baseaddr);
}


int32_t pe_load_framework_loader(
	void **					baseaddr,
	struct pe_framework_runtime_data *	rtdata,
	uintptr_t *				buffer,
	uint32_t				bufsize,
	uint32_t *				sysflags)
{
	return pe_load_library_impl(
		baseaddr,
		rtdata->hloader,
		buffer,bufsize,
		sysflags);
}


int32_t pe_load_framework_library(
	void **					baseaddr,
	void *					hat,
	const wchar16_t *			atrelname,
	uintptr_t *				buffer,
	uint32_t				bufsize,
	uint32_t *				sysflags)
{
	int32_t			status;
	struct pe_unicode_str	path;
	struct os_oa		oa;
	struct os_iosb		iosb;
	const wchar16_t *	wch;
	const wchar16_t *	wch_cap;
	void *			hdsolib;
	void *			hntdll;
	os_zw_close *		zw_close;
	os_zw_open_file *	zw_open_file;

	/* init */
	if (!(hntdll = pe_get_ntdll_module_handle()))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_close = (os_zw_close *)pe_get_procedure_address(
			hntdll,"ZwClose")))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_open_file = (os_zw_open_file *)pe_get_procedure_address(
			hntdll,"ZwOpenFile")))
		return OS_STATUS_INTERNAL_ERROR;

	/* oa */
	oa.len      = sizeof(struct os_oa);
	oa.root_dir = hat;
	oa.obj_name = &path;
	oa.obj_attr = 0;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* at-relative path */
	wch     = atrelname;
	wch_cap = atrelname + 512;

	for (; *wch && wch<wch_cap; )
		wch++;

	if (*wch)
		return OS_STATUS_NAME_TOO_LONG;

	path.buffer = (wchar16_t *)atrelname;
	path.strlen = sizeof(wchar16_t) * (wch - atrelname);
	path.maxlen = 0;

	/* open at */
	if ((status = zw_open_file(
			&hdsolib,
			OS_SEC_SYNCHRONIZE
				| OS_FILE_READ_ACCESS
				| OS_FILE_READ_ATTRIBUTES,
			&oa,&iosb,
			OS_FILE_SHARE_READ
				| OS_FILE_SHARE_WRITE
				| OS_FILE_SHARE_DELETE,
			OS_FILE_NON_DIRECTORY_FILE)))
		return status;

	/* hoppla */
	status = pe_load_library_impl(
		baseaddr,hdsolib,
		buffer,bufsize,
		sysflags);

	/* all done */
	zw_close(hdsolib);

	return status;
}
