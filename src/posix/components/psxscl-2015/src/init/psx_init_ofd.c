/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_helper.h"
#include "psx_process.h"
#include "psx_fcntl.h"
#include "psx_impl.h"
#include "psx_init.h"
#include "psx_pty.h"
#include "psx.h"

typedef struct ___ofd_aligned_size {
	nt_sync_block	sync[2];
} __ofd_aligned_size;

typedef struct ___ofd_asserted_size {
	struct __ofd	ofd;
} __ofd_asserted_size;

__assert_struct_size(__ofd_aligned_size,__ofd_asserted_size);
__assert_struct_size(__ofd_asserted_size,__ofd_aligned_size);

static int __execve_dbg_dummy = 0;

static int __execve_dbg_helper(void)
{
	return __execve_dbg_dummy;
}

static int __psx_std_file_ofd_type(uint32_t ftype)
{
	if (ftype & NT_FILE_TYPE_PIPE)
		return PSX_FD_OS_PIPE;
	else if (ftype & NT_FILE_TYPE_FILE)
		return PSX_FD_OS_FS_FILE;
	else if (ftype & NT_FILE_TYPE_PTY)
		return PSX_FD_PTY;
	else
		return PSX_FD_OS_DEFAULT;
}

static int __psx_execve_ofd_fini(struct __psx_ctx * ctx)
{
	int32_t		status;
	nt_sbi		sbi;
	size_t		size;
	unsigned	pages;
	int32_t		i;

	while (ctx && __execve_dbg_helper());

	__ntapi->zw_close(rtdata->hprocess_parent);

	/* f-i-n-e */
	__ntapi->zw_close(rtdata->uclose[PSX_RTDATA_UPTR_SECTION_FD]);
	__ntapi->zw_close(rtdata->uclose[PSX_RTDATA_UPTR_SECTION_FD_BITMAP]);
	__ntapi->zw_close(rtdata->uclose[PSX_RTDATA_UPTR_SECTION_OFD]);
	__ntapi->zw_close(rtdata->uclose[PSX_RTDATA_UPTR_SECTION_OFD_BITMAP]);

	/* fd tables */
	if ((status = __ntapi->zw_query_section(
			ctx->fd_sec,
			NT_SECTION_BASIC_INFORMATION,
			&sbi,sizeof(sbi),&size)))
		return status;

	size = ctx->fd_cap * sizeof(struct __fd);

	if ((status = __psx_map_primary_section(ctx->fd_sec,(void **)&ctx->fd_slots,&size)))
		return status;

	if ((status = __psx_swap_primary_section(
			ctx->fd_sec,ctx->fd_slots,
			&ctx->fd_sec,(void **)&ctx->fd_slots,
			sbi.section_size.quad,
			size)))
		return status;

	/* fd bitmaps */
	if ((status = __ntapi->zw_query_section(
			ctx->fd_bitmap_sec,
			NT_SECTION_BASIC_INFORMATION,
			&sbi,sizeof(sbi),&size)))
		return status;

	pages = ctx->fd_cap % __PSX_BITS_PER_PAGE
		? (ctx->fd_cap / __PSX_BITS_PER_PAGE) +1
		: (ctx->fd_cap / __PSX_BITS_PER_PAGE);

	size = pages * __PSX_VIRTUAL_PAGE_SIZE;

	if ((status = __psx_map_primary_section(ctx->fd_bitmap_sec,(void **)&ctx->fd_bitmap_addr,&size)))
		return status;

	if ((status = __psx_swap_primary_section(
			ctx->fd_bitmap_sec,ctx->fd_bitmap_addr,
			&ctx->fd_bitmap_sec,(void **)&ctx->fd_bitmap_addr,
			sbi.section_size.quad,
			size)))
		return status;

	/* fd context */
	ctx->fd_blt_ctx = ctx->fd_blt_ctx_array;

	if ((status = __psx_fd_ctx_from_bitmap(ctx)))
		return status;

	/* ofd tables */
	if ((status = __ntapi->zw_query_section(
			ctx->ofd_sec,
			NT_SECTION_BASIC_INFORMATION,
			&sbi,sizeof(sbi),&size)))
		return status;

	size = ctx->ofd_cap * sizeof(struct __ofd);

	if ((status = __psx_map_primary_section(ctx->ofd_sec,(void **)&ctx->ofd_slots,&size)))
		return status;

	if ((status = __psx_swap_primary_section(
			ctx->ofd_sec,ctx->ofd_slots,
			&ctx->ofd_sec,(void **)&ctx->ofd_slots,
			sbi.section_size.quad,
			size)))
		return status;

	/* ofd bitmaps */
	if ((status = __ntapi->zw_query_section(
			ctx->ofd_bitmap_sec,
			NT_SECTION_BASIC_INFORMATION,
			&sbi,sizeof(sbi),&size)))
		return status;

	pages = ctx->ofd_cap % __PSX_BITS_PER_PAGE
		? (ctx->ofd_cap / __PSX_BITS_PER_PAGE) +1
		: (ctx->ofd_cap / __PSX_BITS_PER_PAGE);

	size = pages * __PSX_VIRTUAL_PAGE_SIZE;

	if ((status = __psx_map_primary_section(ctx->ofd_bitmap_sec,(void **)&ctx->ofd_bitmap_addr,&size)))
		return status;

	if ((status = __psx_swap_primary_section(
			ctx->ofd_bitmap_sec,ctx->ofd_bitmap_addr,
			&ctx->ofd_bitmap_sec,(void **)&ctx->ofd_bitmap_addr,
			sbi.section_size.quad,
			size)))
		return status;

	/* ofd context */
	ctx->ofd_blt_ctx = ctx->ofd_blt_ctx_array;

	if ((status = __psx_ofd_ctx_from_bitmap(ctx)))
		return status;

	/* close-on-exec */
	for (i=0; i<ctx->ofd_cap; i++) {
		ctx->ofd_slots[i].info.hevent = 0;

		if (ctx->ofd_slots[i].info.fdtype >= PSX_FD_TYPE_CAP)
			ctx->ofd_slots[i].info.fdtype = 0;
	}

	for (i=0; i<ctx->fd_cap; i++)
		if (ctx->fd_slots[i].flags & FD_CLOEXEC)
			__psx_fd_free(ctx,&ctx->fd_slots[i]);


	/* state */
	__psx.__flags |= PSX_CTX_EXEC_CHILD;

	return NT_STATUS_SUCCESS;
}

static int __psx_ofd_stdio_dup(void ** hstd, struct __ofd * ofd)
{
	int32_t		status;

	if (!ofd->info.fdtype)
		return NT_STATUS_SUCCESS;

	if ((status = __ntapi->zw_duplicate_object(
			rtdata->hprocess_self,
			ofd->info.hfile,
			rtdata->hprocess_self,
			&ofd->info.hfile,
			0,0,NT_DUPLICATE_SAME_ACCESS|NT_DUPLICATE_SAME_ATTRIBUTES)))
		return status;

	__ntapi->zw_close(*hstd);
	*hstd = NT_INVALID_HANDLE_VALUE;

	return NT_STATUS_SUCCESS;
}

int __psx_init_ofd(struct __psx_ctx * ctx)
{
	int32_t		status;
	size_t		size;
	intptr_t	blkid;
	struct __ofd *	ofd;
	struct __ofd *	ptyofd;
	nt_peb *	peb;
	int32_t		i;

	nt_sqos sqos = {
		sizeof(sqos),
		NT_SECURITY_IMPERSONATION,
		NT_SECURITY_TRACKING_DYNAMIC,
		1};

	nt_oa oa = {sizeof(oa),
		    0,0,NT_OBJ_INHERIT,0,(nt_sqos *)&sqos};

	ctx->fd_sec		= rtdata->uptr[PSX_RTDATA_UPTR_SECTION_FD];
	ctx->fd_bitmap_sec	= rtdata->uptr[PSX_RTDATA_UPTR_SECTION_FD_BITMAP];
	ctx->ofd_sec		= rtdata->uptr[PSX_RTDATA_UPTR_SECTION_OFD];
	ctx->ofd_bitmap_sec	= rtdata->uptr[PSX_RTDATA_UPTR_SECTION_OFD_BITMAP];

	ctx->fd_cap		= rtdata->udat32[PSX_RTDATA_UDAT32_FD_CAP];
	ctx->ofd_cap		= rtdata->udat32[PSX_RTDATA_UDAT32_OFD_CAP];

	/* execve? */
	if (ctx->fd_sec)
		return __psx_execve_ofd_fini(ctx);

	/* fd tables */
	size = __PSX_OFD_CAP * sizeof(struct __fd);

	if ((status = __psx_create_primary_section(&ctx->fd_sec,size)))
		return status;

	size = __PSX_OFD_PER_BUCKET * sizeof(struct __fd);

	if ((status = __psx_map_cow_section(ctx->fd_sec,(void **)&ctx->fd_slots,&size)))
		return status;

	/* ofd tables */
	size = __PSX_OFD_CAP * sizeof(struct __ofd);

	if ((status = __psx_create_primary_section(&ctx->ofd_sec,size)))
		return status;

	size = __PSX_OFD_PER_BUCKET * sizeof(struct __ofd);

	if ((status = __psx_map_cow_section(ctx->ofd_sec,(void **)&ctx->ofd_slots,&size)))
		return status;

	/* fd bitmaps */
	if ((status = __psx_create_primary_section(
			&ctx->fd_bitmap_sec,
			__PSX_OFD_BITMAP_PAGES*__PSX_VIRTUAL_PAGE_SIZE)))
		return status;

	size = __PSX_VIRTUAL_PAGE_SIZE;

	if ((status = __psx_map_cow_section(ctx->fd_bitmap_sec,&ctx->fd_bitmap_addr,&size)))
		return status;

	if ((status = __psx_blt_alloc(
			&ctx->fd_blt_ctx_array[0],
			ctx->fd_bitmap_addr,
			ctx->fd_slots,
			sizeof(struct __ofd),
			NT_BLITTER_ENABLE_BLOCK_ARRAY)))
		return status;

	ctx->fd_blt_ctx = ctx->fd_blt_ctx_array;

	/* ofd bitmaps */
	if ((status = __psx_create_primary_section(
			&ctx->ofd_bitmap_sec,
			__PSX_OFD_BITMAP_PAGES*__PSX_VIRTUAL_PAGE_SIZE)))
		return status;

	if ((status = __psx_map_cow_section(ctx->ofd_bitmap_sec,&ctx->ofd_bitmap_addr,&size)))
		return status;

	if ((status = __psx_blt_alloc(
			&ctx->ofd_blt_ctx_array[0],
			ctx->ofd_bitmap_addr,
			ctx->ofd_slots,
			sizeof(struct __ofd),
			NT_BLITTER_ENABLE_BLOCK_ARRAY)))
		return status;

	ctx->ofd_blt_ctx = ctx->ofd_blt_ctx_array;

	/* initialize */
	for (i=0; i<__PSX_OFD_PER_BUCKET; i++)
		ctx->fd_slots[i].invalid = 1;

	/* stdio */
	ofd = ctx->ofd_slots;
	ofd->info.refcnt = 1;
	ofd->info.hfile  = rtdata->hstdin;
	ofd->info.fdtype = __psx_std_file_ofd_type(rtdata->stdin_type);

	ofd++;
	ofd->info.refcnt = 1;
	ofd->info.hfile  = rtdata->hstdout;
	ofd->info.fdtype = __psx_std_file_ofd_type(rtdata->stdout_type);

	ofd++;
	ofd->info.refcnt = 1;
	ofd->info.hfile  = rtdata->hstderr;
	ofd->info.fdtype = __psx_std_file_ofd_type(rtdata->stderr_type);

	ctx->fd_slots[1].ofdidx = 1;
	ctx->fd_slots[2].ofdidx = 2;

	ctx->fd_slots[0].invalid = 0;
	ctx->fd_slots[1].invalid = 0;
	ctx->fd_slots[2].invalid = 0;

	ctx->fd_cap  = __PSX_OFD_PER_BUCKET;
	ctx->ofd_cap = __PSX_OFD_PER_BUCKET;

	for (i=0; i<3; i++) {
		if ((status = __ntapi->blt_acquire(ctx->fd_blt_ctx[0],&blkid)))
			return status;
		else if (i != blkid)
			return NT_STATUS_INTERNAL_ERROR;
	}

	for (i=0; i<3; i++) {
		if ((status = __ntapi->tt_create_private_event(
				&ctx->ofd_slots[i].info.hevent,
				NT_NOTIFICATION_EVENT,
				NT_EVENT_NOT_SIGNALED)))
			return status;

		else if ((status = __ntapi->blt_acquire(ctx->ofd_blt_ctx[0],&blkid)))
			return status;

		else if (i != blkid)
			return NT_STATUS_INTERNAL_ERROR;
	}

	peb = (nt_peb *)pe_get_peb_address();

	if ((status = __psx_ofd_stdio_dup(&peb->process_params->hstdin,&ctx->ofd_slots[0])))
		return status;

	if ((status = __psx_ofd_stdio_dup(&peb->process_params->hstdin,&ctx->ofd_slots[1])))
		return status;

	if ((status = __psx_ofd_stdio_dup(&peb->process_params->hstdin,&ctx->ofd_slots[2])))
		return status;

	/* ad-hoc support of pipe-based terminal emulators */
	for (i=0,ptyofd=0,ofd=ctx->ofd_slots; i<3; i++,ofd++)
		if (ofd->info.fdtype == PSX_FD_PTY) {
			ofd->info.fdtype = PSX_FD_OS_DEFAULT;
			__psx_ofd_free(ctx,ofd);

			if (ptyofd) {
				ctx->fd_slots[i].ofdidx = (int32_t)(ptyofd-ctx->ofd_slots);
				ptyofd->info.refcnt++;
			} else if (!(ptyofd = __psx_pts_open(ctx,0,0)))
				return NT_STATUS_UNEXPECTED_IO_ERROR;
		}

	rtctx.ctty = ptyofd;

	return NT_STATUS_SUCCESS;
}
