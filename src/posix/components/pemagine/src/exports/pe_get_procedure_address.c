/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <pemagine/pemagine.h>

void * pe_get_procedure_address(const void * base, const char * name)
{
	struct pe_export_sym sym;

	return pe_get_export_symbol_info(base,name,&sym)
		? 0 : sym.addr;

}
