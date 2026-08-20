#ifndef _NT_IPC_H_
#define _NT_IPC_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef int32_t __stdcall	ntapi_ipc_create_pipe(
	__out		void **		hpipe_read,
	__out		void **		hpipe_write,
	__in		uint32_t	advisory_buffer_size	__optional);

#endif
