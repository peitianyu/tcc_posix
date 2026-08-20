#ifndef _NT_UUID_H_
#define _NT_UUID_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef int32_t __stdcall ntapi_zw_allocate_locally_unique_id(
	__out	nt_luid *	luid);


/* insufficient for version 4 uuid's */
typedef int32_t __stdcall ntapi_zw_allocate_uuids(
	__out	nt_large_integer *	uuid_last_time_allocated,
	__out	uint32_t *		uuid_delta_time,
	__out	uint32_t *		uuid_sequence_number,
	__out	unsigned char *		uuid_seed);


typedef int32_t __stdcall ntapi_zw_set_uuid_seed(
	__in	unsigned char *		uuid_seed);

#endif
