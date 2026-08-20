/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "pe_os.h"

int32_t pe_find_framework_loader(
	struct pe_framework_runtime_data *	rtdata,
	const wchar16_t *			basename,
	const wchar16_t *			rrelname,
	void *					refaddr,
	uintptr_t *				buffer,
	uint32_t				bufsize,
	uint32_t				flags)
{
	int32_t			status;
	struct pe_unicode_str	path;
	struct os_oa		oa;
	struct os_iosb		iosb;
	void *			himage;
	void *			himgdir;
	void *			hdsodir;
	void *			hloader;
	void *			hparent;
	void *			hprevious;
	const wchar16_t *	wch;
	const wchar16_t *	wch_cap;
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

	/* flags */
	if (flags == PE_LDSO_INTEGRAL_ONLY)
		(void)0;
	else if (flags == PE_LDSO_DEFAULT_EXECUTABLE)
		(void)0;
	else if (flags == PE_LDSO_STANDALONE_EXECUTABLE)
		(void)0;
	else
		return OS_STATUS_INVALID_PARAMETER;

	/* oa */
	oa.len      = sizeof(struct os_oa);
	oa.root_dir = 0;
	oa.obj_name = &path;
	oa.obj_attr = OS_OBJ_CASE_INSENSITIVE;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* standalone executable? */
	if (flags == PE_LDSO_STANDALONE_EXECUTABLE) {
		wch     = basename;
		wch_cap = basename + 512;

		for (; *wch && wch<wch_cap; wch++)
			if ((*wch == '\\') || (*wch == '/'))
				return OS_STATUS_INVALID_PARAMETER;

		if (*wch)
			return OS_STATUS_NAME_TOO_LONG;

		path.buffer = (wchar16_t *)basename;
		path.strlen = sizeof(wchar16_t) * (wch - basename);
		path.maxlen = 0;

		if ((status = pe_open_image_from_addr(
				&himage,refaddr,
				buffer,bufsize,
				0,
				OS_SEC_SYNCHRONIZE
					| OS_FILE_READ_ACCESS
					| OS_FILE_READ_ATTRIBUTES,
				OS_FILE_SHARE_READ
					| OS_FILE_SHARE_WRITE
					| OS_FILE_SHARE_DELETE,
				0)))
			return status;

		if ((status = pe_open_physical_parent_directory(
				&hdsodir,himage,
				buffer,bufsize,
				0,
				OS_SEC_SYNCHRONIZE
					| OS_FILE_READ_ACCESS
					| OS_FILE_READ_ATTRIBUTES,
				OS_FILE_SHARE_READ
					| OS_FILE_SHARE_WRITE
					| OS_FILE_SHARE_DELETE,
				0)))
			return status;

		oa.root_dir = hdsodir;

		if ((status = zw_open_file(
				&hloader,
				OS_SEC_SYNCHRONIZE
					| OS_FILE_READ_ACCESS
					| OS_FILE_READ_ATTRIBUTES,
				&oa,&iosb,
				OS_FILE_SHARE_READ
					| OS_FILE_SHARE_WRITE
					| OS_FILE_SHARE_DELETE,
				OS_FILE_NON_DIRECTORY_FILE)))
			return status;

		if (rtdata->hloader)
			zw_close(hloader);
		else
			rtdata->hloader = hloader;

		rtdata->hdsodir = hdsodir;
		rtdata->himage  = himage;

		return OS_STATUS_SUCCESS;
	}

	/* root-relative loader path */
	wch     = rrelname;
	wch_cap = rrelname + 512;

	for (; *wch && wch<wch_cap; )
		wch++;

	if (*wch)
		return OS_STATUS_NAME_TOO_LONG;

	path.buffer = (wchar16_t *)rrelname;
	path.strlen = sizeof(wchar16_t) * (wch - rrelname);
	path.maxlen = 0;

	/* inherited root directory? */
	if (rtdata->hroot) {
		oa.root_dir = rtdata->hroot;

		if ((status = zw_open_file(
				&hloader,
				OS_SEC_SYNCHRONIZE
					| OS_FILE_READ_ACCESS
					| OS_FILE_READ_ATTRIBUTES,
				&oa,&iosb,
				OS_FILE_SHARE_READ
					| OS_FILE_SHARE_WRITE
					| OS_FILE_SHARE_DELETE,
				OS_FILE_NON_DIRECTORY_FILE)))
			return status;

		if (rtdata->hloader)
			zw_close(hloader);
		else
			rtdata->hloader = hloader;

		rtdata->hdsodir = 0;

		return OS_STATUS_SUCCESS;
	}

	/* integral only and no inherited root directory? */
	if (flags == PE_LDSO_INTEGRAL_ONLY)
		return OS_STATUS_COULD_NOT_INTERPRET;

	/* finde Pluto / find Waldo */
	if ((status = pe_open_image_from_addr(
			&himage,refaddr,
			buffer,bufsize,
			0,
			OS_SEC_SYNCHRONIZE
				| OS_FILE_READ_ACCESS
				| OS_FILE_READ_ATTRIBUTES,
			OS_FILE_SHARE_READ
				| OS_FILE_SHARE_WRITE
				| OS_FILE_SHARE_DELETE,
			0)))
		return status;

	if ((status = pe_open_physical_parent_directory(
			&himgdir,himage,
			buffer,bufsize,
			0,
			OS_SEC_SYNCHRONIZE
				| OS_FILE_READ_ACCESS
				| OS_FILE_READ_ATTRIBUTES,
			OS_FILE_SHARE_READ
				| OS_FILE_SHARE_WRITE
				| OS_FILE_SHARE_DELETE,
			0)))
		return status;

	if ((status = pe_open_physical_parent_directory(
			&hparent,himgdir,
			buffer,bufsize,
			OS_OBJ_INHERIT,
			OS_SEC_SYNCHRONIZE
				| OS_FILE_READ_ACCESS
				| OS_FILE_READ_ATTRIBUTES,
			OS_FILE_SHARE_READ
				| OS_FILE_SHARE_WRITE
				| OS_FILE_SHARE_DELETE,
			0)))
		return status;

	zw_close(himgdir);

	hloader     = 0;
	oa.root_dir = hparent;

	status = zw_open_file(
		&hloader,
		OS_SEC_SYNCHRONIZE
			| OS_FILE_READ_ACCESS
			| OS_FILE_READ_ATTRIBUTES,
		&oa,&iosb,
		OS_FILE_SHARE_READ
			| OS_FILE_SHARE_WRITE
			| OS_FILE_SHARE_DELETE,
		OS_FILE_NON_DIRECTORY_FILE);

	while (!hloader) {
		if (status == OS_STATUS_OBJECT_NAME_NOT_FOUND)
			(void)0;
		else if (status == OS_STATUS_OBJECT_PATH_NOT_FOUND)
			(void)0;
		else
			return status;

		hprevious = hparent;

		if ((status = pe_open_physical_parent_directory(
				&hparent,hprevious,
				buffer,bufsize,
				OS_OBJ_INHERIT,
				OS_SEC_SYNCHRONIZE
					| OS_FILE_READ_ACCESS
					| OS_FILE_READ_ATTRIBUTES,
				OS_FILE_SHARE_READ
					| OS_FILE_SHARE_WRITE
					| OS_FILE_SHARE_DELETE,
				0)))
			return status;

		oa.root_dir = hparent;

		status = zw_open_file(
			&hloader,
			OS_SEC_SYNCHRONIZE
				| OS_FILE_READ_ACCESS
				| OS_FILE_READ_ATTRIBUTES,
			&oa,&iosb,
			OS_FILE_SHARE_READ
				| OS_FILE_SHARE_WRITE
				| OS_FILE_SHARE_DELETE,
			OS_FILE_NON_DIRECTORY_FILE);

		zw_close(hprevious);
	}

	if (rtdata->hloader)
		zw_close(hloader);
	else
		rtdata->hloader = hloader;

	rtdata->hdsodir = 0;
	rtdata->himage  = himage;
	rtdata->hroot   = hparent;

	return OS_STATUS_SUCCESS;
}
