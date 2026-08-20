#ifndef _NT_HASH_H_
#define _NT_HASH_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_hash.h>

typedef int32_t __cdecl ntapi_tt_populate_hashed_import_table(
    __in 	void *			image_base,
    __in	void *			import_table,
    __in	ntapi_hashed_symbol *	hash_table,
    __in	uint32_t		hash_table_array_size);

#endif
