/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pe_consts.h>
#include <pemagine/pe_structs.h>
#include <pemagine/pemagine.h>


struct pe_raw_data_dirs * pe_get_image_data_dirs_addr(const void * base)
{
	uint16_t *		magic;
	union pe_raw_opt_hdr *	hdr;

	if (!(hdr = pe_get_image_opt_hdr_addr(base)))
		return 0;

	magic = (uint16_t *)hdr;

	switch (*magic) {
		case PE_MAGIC_PE32:
			return (struct pe_raw_data_dirs *)hdr->opt_hdr_32.coh_rva_and_sizes;

		case PE_MAGIC_PE32_PLUS:
			return (struct pe_raw_data_dirs *)hdr->opt_hdr_64.coh_rva_and_sizes;

		default:
			return 0;
	}
}
