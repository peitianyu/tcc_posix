/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "pe_os.h"

int32_t pe_terminate_current_process(int32_t estatus)
{
	void *				hntdll;
	os_zw_terminate_process *	zw_terminate_process;

	/* init */
	if (!(hntdll = pe_get_ntdll_module_handle()))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_terminate_process = (os_zw_terminate_process *)pe_get_procedure_address(
			hntdll,"ZwTerminateProcess")))
		return OS_STATUS_INTERNAL_ERROR;

	return zw_terminate_process(
		OS_CURRENT_PROCESS_HANDLE,
		estatus);
}
