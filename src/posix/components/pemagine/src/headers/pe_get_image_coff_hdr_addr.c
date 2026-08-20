/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pe_consts.h>
#include <pemagine/pe_structs.h>
#include <pemagine/pemagine.h>


struct pe_raw_coff_image_hdr * pe_get_image_coff_hdr_addr(const void * base)
{
	struct pe_raw_image_dos_hdr *	dos;
	struct pe_raw_coff_image_hdr *	coff;
	uint32_t *			offset;

	if (!(dos = pe_get_image_dos_hdr_addr(base)))
		return 0;

	offset = (uint32_t *)(dos->dos_lfanew);
	coff   = (struct pe_raw_coff_image_hdr *)pe_va_from_rva(base,*offset);

	return ((coff->cfh_signature[0] == 'P')
			&& (coff->cfh_signature[1] == 'E')
			&& (coff->cfh_signature[2] == '\0')
			&& (coff->cfh_signature[3] == '\0'))
		? coff : 0;
}
