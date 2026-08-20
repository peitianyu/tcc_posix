/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <pemagine/pe_structs.h>
#include "pe_os.h"

pe_api int32_t pe_open_image_from_addr(
	__out	void **			himage,
	__in	void *			addr,
	__out	uintptr_t *		buffer,
	__in	size_t			bufsize,
	__in	uint32_t		oattr,
	__in	uint32_t		desired_access,
	__in	uint32_t		share_access,
	__in	uint32_t		open_options)
{
	struct os_oa			oa;
	struct os_iosb			iosb;
	void *				hntdll;
	os_zw_open_file *		zw_open_file;
	struct pe_unicode_str		path;
	struct pe_ldr_tbl_entry *	lentry;
	wchar16_t *			name;
	wchar16_t *			cap;
	wchar16_t *			src;
	wchar16_t *			dst;
	wchar16_t *			mark;

	/* init */
	if (!(hntdll = pe_get_ntdll_module_handle()))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_open_file = (os_zw_open_file *)pe_get_procedure_address(
			hntdll,"ZwOpenFile")))
		return OS_STATUS_INTERNAL_ERROR;

	/* native path of image containing addr */
	if (!(lentry = pe_get_ldr_entry_from_addr(addr)))
		return OS_STATUS_INTERNAL_ERROR;

	if (bufsize - 4*sizeof(wchar16_t) < lentry->full_dll_name.strlen)
		return OS_STATUS_BUFFER_TOO_SMALL;

	name = lentry->full_dll_name.buffer;
	mark = (wchar16_t *)buffer;
	dst  = mark;

	if (name[1] == ':') {
		*dst++ = '\\';
		*dst++ = '?';
		*dst++ = '?';
		*dst++ = '\\';
	}

	src = name;
	cap  = &dst[lentry->full_dll_name.strlen / sizeof(wchar16_t)];

	for (; dst<cap; )
		*dst++ = *src++;

	*dst = 0;

	path.strlen = sizeof(wchar16_t) * (cap - mark);
	path.maxlen = 0;
	path.buffer = mark;

	/* oa */
	oa.len      = sizeof(struct os_oa);
	oa.root_dir = 0;
	oa.obj_name = &path;
	oa.obj_attr = oattr;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* default access */
	desired_access = desired_access
		? desired_access
		: OS_SEC_SYNCHRONIZE | OS_FILE_READ_ATTRIBUTES | OS_FILE_READ_ACCESS;

	/* open image */
	return zw_open_file(
		himage,
		desired_access,
		&oa,&iosb,
		share_access,
		open_options | OS_FILE_NON_DIRECTORY_FILE);
}
