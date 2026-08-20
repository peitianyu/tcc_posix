/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pe_consts.h>
#include <pemagine/pe_structs.h>
#include <pemagine/pemagine.h>
#include "pe_impl.h"


void * pe_get_image_special_hdr_addr(const void * base, uint32_t ordinal, uint32_t * sec_size)
{
	struct pe_raw_data_dirs*dirs;
	struct pe_block *	dir;
	uint32_t *		count;

	if (!(dirs = pe_get_image_data_dirs_addr(base)))
		return 0;

	count = (uint32_t *)dirs->coh_rva_and_sizes;

	if (*count < (ordinal+1))
		return 0;

	dir  = (struct pe_block *)dirs->coh_export_tbl;
	dir += ordinal;

	if (sec_size)
		*sec_size = dir->size;

	return dir->rva
		? pe_va_from_rva(base,dir->rva)
		: 0;
}


struct pe_raw_export_hdr * pe_get_image_export_hdr_addr(const void * base, uint32_t * sec_size)
{
	return (struct pe_raw_export_hdr *)pe_get_image_special_hdr_addr(base,PE_IMAGE_DATA_DIR_ORDINAL_EXPORT,sec_size);
}


struct pe_raw_import_hdr * pe_get_image_import_dir_addr(const void * base, uint32_t * sec_size)
{
	return (struct pe_raw_import_hdr *)pe_get_image_special_hdr_addr(base,PE_IMAGE_DATA_DIR_ORDINAL_IMPORT,sec_size);
}
