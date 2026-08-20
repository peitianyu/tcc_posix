#ifndef _PSX_CWD_H_
#define _PSX_CWD_H_

#include "psx_systypes.h"
#include "psx_impl.h"
#include "psx.h"

int32_t __psx_getcwd(struct __psx_ctx *, char * buf, size_t size);
int32_t __psx_setcwd(struct __psx_ctx *, struct __path_info * path_info);

#endif
