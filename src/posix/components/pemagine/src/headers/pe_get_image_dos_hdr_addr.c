/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>

#include <pemagine/pe_consts.h>
#include <pemagine/pe_structs.h>
#include <pemagine/pemagine.h>


struct pe_raw_image_dos_hdr * pe_get_image_dos_hdr_addr(const void * base)
{
	struct pe_raw_image_dos_hdr * dos;

	dos = (struct pe_raw_image_dos_hdr *)base;

	return ((dos->dos_magic[0] == 'M') && (dos->dos_magic[1] == 'Z'))
		? dos : 0;
}
