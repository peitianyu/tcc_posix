#ifndef _PSX_PTY_H_
#define _PSX_PTY_H_

#include <ntapi/ntapi.h>

struct __ofd * __psx_dbs_open(struct __psx_ctx *, uint32_t psxflags);
struct __ofd * __psx_ptm_open(struct __psx_ctx *, uint32_t psxflags);
struct __ofd * __psx_pts_open(struct __psx_ctx *, nt_pty * hptm, uint32_t psxflags);
struct __ofd * __psx_pty_open(struct __psx_ctx *, struct __ofd *);

#endif
