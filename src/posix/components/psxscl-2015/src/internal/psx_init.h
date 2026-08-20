#ifndef _PSX_INIT_H_
#define _PSX_INIT_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_impl.h"

int __psx_init_brk(void);
int __psx_init_cwd(void);
int __psx_init_ctx(void);
int __psx_init_dbg(void);
int __psx_init_env(void);
int __psx_init_tty(void);
int __psx_init_mman(void);
int __psx_init_pgid(void);
int __psx_init_ofd(struct __psx_ctx *);
int __psx_init_signal(struct __psx_ctx *);
int __psx_init_session(struct __psx_ctx *);

#endif
