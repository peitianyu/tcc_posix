#ifndef _PSX_ACCESS_H_
#define _PSX_ACCESS_H_

#include "psx_systypes.h"
#include "psx_stat.h"

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

void __psx_access_convert_native_to_posix(uint32_t naccess, mode_t * xaccess);

#endif
