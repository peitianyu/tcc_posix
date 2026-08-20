/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_section.h>
#include <ntapi/nt_process.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

static nt_sqos const sqos = {
	sizeof(sqos),
	NT_SECURITY_IMPERSONATION,
	NT_SECURITY_TRACKING_DYNAMIC,
	1};

static int32_t __tt_exec_unmap_image(nt_executable_image * image, void * base, int32_t status)
{
	int32_t ret;

	if (base)
		if ((ret = __ntapi->zw_unmap_view_of_section(
				NT_CURRENT_PROCESS_HANDLE,
				base)))
			return ret;

	if (image->hsection)
		if ((ret = __ntapi->zw_close(image->hsection)))
			return ret;

	return status;
}

int32_t	__stdcall __ntapi_tt_exec_unmap_image(nt_executable_image * image)
{
	return __tt_exec_unmap_image(image,image->addr,0);
}


int32_t __stdcall __ntapi_tt_exec_map_image_as_data(nt_executable_image * image)
{
	int32_t				status;
	uint16_t *			pi16;
	uint32_t *			pi32;
	nt_sec_size			sec_size;
	size_t				view_size;
	void *				base;
	void *				hsection;

	struct pe_image_dos_hdr *	dos;
	struct pe_coff_file_hdr *	coff;
	union  pe_opt_hdr *		opt;
	struct pe_sec_hdr *		sec;

	nt_oa oa = {sizeof(oa),
		    0,0,0,0,(nt_sqos *)&sqos};

	base = 0;
	sec_size.quad = 0;
	view_size = image->size;

	if ((status = __ntapi->zw_create_section(
			&hsection,
			NT_SECTION_MAP_READ,
			&oa,
			&sec_size,
			NT_PAGE_READONLY,
			NT_SEC_RESERVE,image->hfile)))
		return status;

	if ((status = __ntapi->zw_map_view_of_section(
			hsection,
			NT_CURRENT_PROCESS_HANDLE,
			&base,
			0,0,0,
			&view_size,
			NT_VIEW_UNMAP,0,
			NT_PAGE_READONLY)))
		return __tt_exec_unmap_image(
			image,base,status);

	if (!(dos = pe_get_image_dos_hdr_addr(base)))
		return 0;

	pi32 = (uint32_t *)dos->dos_lfanew;
	if ((*pi32 + sizeof(*coff)) > view_size)
		return __tt_exec_unmap_image(
			image,base,NT_STATUS_INVALID_IMAGE_FORMAT);

	if (!(coff = pe_get_image_coff_hdr_addr(base)))
		return 0;

	if (!(opt = pe_get_image_opt_hdr_addr(base)))
		return 0;

	sec  = pe_get_image_section_tbl_addr(base);
	pi16 = (uint16_t *)coff->num_of_sections;
	if (((size_t)sec-(size_t)base + *pi16 * sizeof(*sec)) > view_size)
		return __tt_exec_unmap_image(
			image,base,NT_STATUS_INVALID_IMAGE_FORMAT);

	/* subsystem: same offset (pe32, pe32+) */
	pi16 = (uint16_t *)opt;
	image->magic = *pi16;

	pi16 = (uint16_t *)opt->opt_hdr_32.subsystem;
	image->subsystem = *pi16;

	pi16 = (uint16_t *)coff->characteristics;
	image->characteristics = *pi16;

	image->hsection = hsection;
	image->addr = base;
	image->size = view_size;

	return status;
}
