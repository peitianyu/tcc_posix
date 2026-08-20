#ifndef _PSX_OFD_H_
#define _PSX_OFD_H_

#include <ntapi/ntapi.h>

struct __psx_ctx;

enum __psx_fd_type {
	PSX_FD_OS_DEFAULT,

	PSX_FD_OS_FS_FILE,
	PSX_FD_OS_FS_DIR,
	PSX_FD_OS_FS_ROOT,

	PSX_FD_OS_BNO_OBJ,
	PSX_FD_OS_BNO_DIR,
	PSX_FD_OS_BNO_ROOT,

	PSX_FD_OS_REG_ENTRY,
	PSX_FD_OS_REG_DIR,
	PSX_FD_OS_REG_ROOT,

	PSX_FD_OS_PIPE,
	PSX_FD_OS_SOCKET,
	PSX_FD_OS_MAILSLOT,
	PSX_FD_OS_CONFIG,
	PSX_FD_OS_DEVICE,
	PSX_FD_OS_PROCFS,
	PSX_FD_OS_MOUNT_POINT,

	PSX_FD_UDP_SOCKET,
	PSX_FD_PTY,
	PSX_FD_VFD,

	PSX_FD_DEV_NULL,
	PSX_FD_DEV_ZERO,
	PSX_FD_DEV_PTMX,
	PSX_FD_DEV_PTS,
	PSX_FD_DEV_TTY,
	PSX_FD_DEV_RANDOM,
	PSX_FD_DEV_URANDOM,
	PSX_FD_DEV_MNTMGR,

	PSX_FD_PROC_SELF,
	PSX_FD_PROC_NET,
	PSX_FD_PROC_SYS,
	PSX_FD_PROC_SYSVIPC,
	PSX_FD_PROC_REGISTRY,

	PSX_FD_TYPE_CAP
};


struct __ofd_bucket {
	void *	hsection;
	void *	addr;
};


struct __ofd {
	nt_socket	info;
	nt_sync_block	sync;
};


struct __fd {
	int32_t		ofdidx;
	uint32_t	flags;
	int32_t		refcnt;
	int32_t		invalid;
};


struct __pollofd
{
	struct __ofd *	ofd;
	short		events;
	short		revents;
	uint32_t	flags;
};


int32_t		__psx_fd_ctx_from_bitmap	(struct __psx_ctx * ctx);
int32_t		__psx_ofd_ctx_from_bitmap	(struct __psx_ctx * ctx);
struct __fd *	__psx_fd_alloc			(struct __psx_ctx * ctx, intptr_t * idx);
struct __fd *	__psx_fd_obtain			(struct __psx_ctx * ctx, intptr_t * idx);
struct __fd *	__psx_fd_possess		(struct __psx_ctx * ctx, intptr_t * idx);
int32_t		__psx_fd_free			(struct __psx_ctx * ctx, struct __fd * fd);
struct __fd *	__psx_fd_ref_inc		(struct __psx_ctx * ctx, intptr_t fdidx);
void		__psx_fd_ref_dec		(struct __psx_ctx * ctx, struct __fd * fd);
struct __ofd *	__psx_ofd_alloc			(struct __psx_ctx * ctx, intptr_t * idx);
void		__psx_ofd_free			(struct __psx_ctx * ctx, struct __ofd * ofd);
struct __ofd *	__psx_ofd_ref_inc		(struct __psx_ctx * ctx, intptr_t fdidx);
void		__psx_ofd_ref_dec		(struct __psx_ctx * ctx, struct __ofd * ofd);

#endif
