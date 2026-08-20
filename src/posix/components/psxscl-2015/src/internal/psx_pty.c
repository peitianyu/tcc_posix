/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_fcntl.h"
#include "psx_impl.h"
#include "psx_pty.h"
#include "psx.h"

static struct __ofd * __pty_open(struct __psx_ctx * ctx, nt_pty * hptm, nt_guid * pty_guid, uint32_t psxflags)
{
	int32_t			status;
	struct __ofd *		ofd;
	intptr_t		ofdidx;
	nt_pty *		hpty;
	nt_vfd_dev_name		pty_name;
	nt_pty_client_info	ptyinfo;
	nt_iosb			iosb;
	uint32_t		options;
	nt_oa			oa = {sizeof(oa)};

	if (!(ofd = __psx_ofd_alloc(ctx,&ofdidx)))
		return 0;

	__ntapi->vfd_dev_name_init(
		&pty_name,pty_guid);

	oa.obj_name = &pty_name.name;
	oa.root_dir = hptm;

	options = (psxflags & O_NOCTTY) ? 0 : NT_FILE_SESSION_AWARE;

	if ((status = __ntapi->pty_open(0,&hpty,NT_FILE_ALL_ACCESS,&oa,0,0,options)))
		return 0;

	ptyinfo.any[0] = ofdidx;
	ptyinfo.any[1] = 0;
	ptyinfo.any[2] = 0;
	ptyinfo.any[3] = 0;

	if ((status = __ntapi->pty_set(hpty,&iosb,&ptyinfo,sizeof(ptyinfo),NT_PTY_CLIENT_INFORMATION))) {
		__ntapi->pty_close(hpty);
		__psx_ofd_free(ctx,ofd);
		return 0;
	}

	ofd->info.hpty   = hpty;
	ofd->info.refcnt = 1;
	ofd->info.fdtype = PSX_FD_PTY;

	return ofd;
}

struct __ofd * __psx_dbg_open(struct __psx_ctx * ctx, uint32_t psxflags)
{
	nt_guid pty_guid = TTY_DBG_GUID;
	return __pty_open(ctx,0,&pty_guid,psxflags);
}

struct __ofd * __psx_ptm_open(struct __psx_ctx * ctx, uint32_t psxflags)
{
	nt_guid pty_guid = TTY_PTM_GUID;
	return __pty_open(ctx,0,&pty_guid,psxflags);
}

struct __ofd * __psx_pts_open(struct __psx_ctx * ctx, nt_pty * hptm, uint32_t psxflags)
{
	nt_guid pty_guid = TTY_PTS_GUID;
	return __pty_open(ctx,hptm,&pty_guid,psxflags);
}
