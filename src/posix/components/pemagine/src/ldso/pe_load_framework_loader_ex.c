/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "pe_os.h"

int32_t pe_load_framework_loader_ex(
	void **					baseaddr,
	void **					hroot,
	void **					hdsodir,
	const struct pe_guid *			abi,
	const wchar16_t *			basename,
	const wchar16_t *			rrelname,
	void *					refaddr,
	uintptr_t *				buffer,
	uint32_t				bufsize,
	uint32_t				flags,
	uint32_t *				sysflags)
{
	int32_t					status;
	struct pe_framework_runtime_data *	rtdata;
	struct pe_framework_runtime_data	context;

	status = pe_get_framework_runtime_data(
		&rtdata,
		pe_get_peb_command_line(),
		abi);

	if (status) {
		rtdata = &context;

		context.hself        = 0;
		context.hparent      = 0;
		context.himage       = 0;
		context.hroot        = 0;
		context.hdsodir      = 0;
		context.hloader      = 0;
		context.hexec        = 0;
		context.hpeer        = 0;
		context.hcwd         = 0;
		context.hdrive       = 0;

		context.abi.data1    = abi->data1;
		context.abi.data1    = abi->data2;
		context.abi.data1    = abi->data3;

		context.abi.data4[0] = abi->data4[0];
		context.abi.data4[1] = abi->data4[1];
		context.abi.data4[2] = abi->data4[2];
		context.abi.data4[3] = abi->data4[3];
		context.abi.data4[4] = abi->data4[4];
		context.abi.data4[5] = abi->data4[5];
		context.abi.data4[6] = abi->data4[6];
		context.abi.data4[7] = abi->data4[7];

		context.hldrctx[PE_LDSO_CTX_IDX_PREV_LOADER] = 0;
		context.hldrctx[PE_LDSO_CTX_IDX_PREV_ROOT]   = 0;

		if (__SIZEOF_POINTER__ == 8) {
			context.hldrctx[PE_LDSO_CTX_IDX_RESERVED_1] = 0;
			context.hldrctx[PE_LDSO_CTX_IDX_RESERVED_2] = 0;
		}
	} else {
		rtdata->hldrctx[PE_LDSO_CTX_IDX_PREV_LOADER] = rtdata->hloader;
		rtdata->hldrctx[PE_LDSO_CTX_IDX_PREV_ROOT]   = rtdata->hroot;
	}

	if ((status = pe_find_framework_loader(
			rtdata,basename,rrelname,refaddr,
			buffer,bufsize,flags)))
		return status;

	if ((status = pe_load_framework_loader(
			baseaddr,rtdata,
			buffer,bufsize,
			sysflags)))
		return status;

	*hroot   = rtdata->hroot;
	*hdsodir = rtdata->hdsodir;

	return OS_STATUS_SUCCESS;
}
