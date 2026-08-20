/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <pemagine/pe_structs.h>
#include "pe_os.h"

int32_t pe_open_physical_parent_directory(
	__out	void **		hparent,
	__in	void *		href,
	__out	uintptr_t *	buffer,
	__in	uint32_t	bufsize,
	__in	uint32_t	oattr,
	__in	uint32_t	desired_access,
	__in	uint32_t	share_access,
	__in	uint32_t	open_options)
{
	int32_t			status;
	struct os_oa		oa;
	struct os_iosb		iosb;
	wchar16_t *		wch;
	wchar16_t *		root;
	struct pe_unicode_str *	path;
	uint32_t		len;
	int			mup;
	void *			hntdll;
	os_zw_query_object *	zw_query_object;
	os_zw_open_file *	zw_open_file;


	/* init */
	path = (struct pe_unicode_str *)buffer;

	if (!(hntdll = pe_get_ntdll_module_handle()))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_query_object = (os_zw_query_object *)pe_get_procedure_address(
			hntdll,"ZwQueryObject")))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_open_file = (os_zw_open_file *)pe_get_procedure_address(
			hntdll,"ZwOpenFile")))
		return OS_STATUS_INTERNAL_ERROR;

	/* native path of base directory */
	if ((status = zw_query_object(
			href,
			OS_OBJECT_NAME_INFORMATION,
			path,
			bufsize,
			&len)))
		return status;

	/* integrity */
	if (len == sizeof(struct pe_unicode_str))
		return OS_STATUS_BAD_FILE_TYPE;

	/* device root directory */
	root = path->buffer;
	wch  = path->buffer + (path->strlen / sizeof(uint16_t));


	if ((wch < &root[8])
			|| (root[0] != '\\')
			|| (root[1] != 'D') || (root[2] != 'e')
			|| (root[3] != 'v') || (root[4] != 'i')
			|| (root[5] != 'c') || (root[6] != 'e')
			|| (root[7] != '\\'))
		return OS_STATUS_INTERNAL_ERROR;

	mup = (wch > &root[11])
		&& (root[8]=='M')
		&& (root[9]=='u')
		&& (root[10]=='p')
		&& (root[11]=='\\');

	root = mup ? &root[12] : &root[8];

	for (; (root<wch) && (*root!='\\'); )
		root++;

	if (root == wch)
		return OS_STATUS_INTERNAL_ERROR;

	if (mup)
		for (root++; (root<wch) && (*root!='\\'); )
			root++;

	if (root == wch)
		return OS_STATUS_INTERNAL_ERROR;

	if (&root[1] == wch)
		return OS_STATUS_MORE_PROCESSING_REQUIRED;

	if (wch[-1] == '\\')
		wch--;

	/* physical parent directory path */
	for (root++; (wch>=root) && (wch[-1]!='\\'); )
		wch--;

	path->strlen = (uint16_t)(wch - path->buffer) * sizeof(uint16_t);
	path->maxlen = 0;

	/* oa */
	oa.len      = sizeof(struct os_oa);
	oa.root_dir = 0;
	oa.obj_name = path;
	oa.obj_attr = oattr;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* default access */
	desired_access = desired_access
		? desired_access
		: OS_SEC_SYNCHRONIZE | OS_FILE_READ_ATTRIBUTES | OS_FILE_READ_ACCESS;

	/* open parent directory */
	return zw_open_file(
		hparent,
		desired_access,
		&oa,&iosb,
		share_access,
		open_options | OS_FILE_DIRECTORY_FILE);
}
