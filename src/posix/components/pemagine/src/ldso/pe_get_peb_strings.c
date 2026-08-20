/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "pe_os.h"

wchar16_t * pe_get_peb_command_line(void)
{
	struct os_peb * peb;

	return (peb = (struct os_peb *)pe_get_peb_address())
		? peb->process_params->command_line.buffer
		: 0;
}


wchar16_t * pe_get_peb_environment_block(void)
{
	struct os_peb * peb;

	return (peb = (struct os_peb *)pe_get_peb_address())
		? peb->process_params->environment
		: 0;
}
