#ifndef _PSX_IO_H_
#define _PSX_IO_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_ofd.h"

enum __io_mode {
	__IO_READ,
	__IO_WRITE
};

__assert_struct_size(off_t,nt_large_integer);
__assert_struct_size(nt_large_integer,off_t);

void __psx_io_set_status(struct __psx_tlca * tlca, struct __ofd *, nt_iosb *);

#endif
