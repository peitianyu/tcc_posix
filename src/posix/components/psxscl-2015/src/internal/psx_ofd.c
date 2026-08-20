/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_ofd.h"
#include "psx_helper.h"
#include "psx_impl.h"

int32_t __psx_fd_ctx_from_bitmap(struct __psx_ctx * ctx)
{
	unsigned	i,pages;
	int32_t		status;
	uintptr_t *	bitmap;
	size_t *	region;

	bitmap = (uintptr_t *)ctx->fd_bitmap_addr;
	region = (size_t *)ctx->fd_slots;

	pages = ctx->ofd_cap % __PSX_BITS_PER_PAGE
		? (ctx->ofd_cap / __PSX_BITS_PER_PAGE) +1
		: (ctx->ofd_cap / __PSX_BITS_PER_PAGE);

	for (i=0; i<pages; i++) {
		if ((status = __psx_blt_alloc(
				&ctx->fd_blt_ctx[i],
				bitmap,region,
				sizeof(struct __fd),
				NT_BLITTER_ENABLE_BLOCK_ARRAY | NT_BLITTER_PRESERVE_BITS)))
			return status;

		bitmap += __PSX_BITS_PER_PAGE / sizeof(uintptr_t);
		region += __PSX_BITS_PER_PAGE / sizeof(uintptr_t) * sizeof(struct __fd);
	}

	return status;
}


int32_t __psx_ofd_ctx_from_bitmap(struct __psx_ctx * ctx)
{
	unsigned	i,pages;
	int32_t		status;
	uintptr_t *	bitmap;
	size_t *	region;

	bitmap = (uintptr_t *)ctx->ofd_bitmap_addr;
	region = (size_t *)ctx->ofd_slots;

	pages = ctx->ofd_cap % __PSX_BITS_PER_PAGE
		? (ctx->ofd_cap / __PSX_BITS_PER_PAGE) +1
		: (ctx->ofd_cap / __PSX_BITS_PER_PAGE);

	for (i=0; i<pages; i++) {
		if ((status = __psx_blt_alloc(
				&ctx->ofd_blt_ctx[i],
				bitmap,region,
				sizeof(struct __ofd),
				NT_BLITTER_ENABLE_BLOCK_ARRAY | NT_BLITTER_PRESERVE_BITS)))
			return status;

		bitmap += __PSX_BITS_PER_PAGE / sizeof(uintptr_t);
		region += __PSX_BITS_PER_PAGE / sizeof(uintptr_t) * sizeof(struct __ofd);
	}

	return status;
}


struct __fd * __psx_fd_alloc(struct __psx_ctx * ctx, intptr_t * idx)
{
	int32_t		status;
	intptr_t	blkid;
	unsigned	i,pages;

	pages = ctx->fd_cap % __PSX_BITS_PER_PAGE
		? (ctx->fd_cap / __PSX_BITS_PER_PAGE) +1
		: (ctx->fd_cap / __PSX_BITS_PER_PAGE);

	for (i=0,status=-1; (i<pages) && status; i++)
		status = __ntapi->blt_acquire(ctx->fd_blt_ctx[i],&blkid);

	if (status) return 0;

	*idx = blkid + (--i*__PSX_BITS_PER_PAGE);
	return &ctx->fd_slots[*idx];
}


struct __fd * __psx_fd_obtain(struct __psx_ctx * ctx, intptr_t * idx)
{
	int32_t		status;
	intptr_t	blkid;
	intptr_t	i,pages;

	pages = ctx->fd_cap % __PSX_BITS_PER_PAGE
		? (ctx->fd_cap / __PSX_BITS_PER_PAGE) +1
		: (ctx->fd_cap / __PSX_BITS_PER_PAGE);

	i     = *idx / __PSX_BITS_PER_PAGE;
	blkid = *idx % __PSX_BITS_PER_PAGE;

	if ((status = __ntapi->blt_obtain(ctx->fd_blt_ctx[i],&blkid)))
		for (blkid=0,++i; (i<pages) && status; i++)
			status = __ntapi->blt_acquire(ctx->fd_blt_ctx[i],&blkid);

	if (status) return 0;

	*idx = blkid + (--i*__PSX_BITS_PER_PAGE);
	return &ctx->fd_slots[*idx];
}


struct __fd * __psx_fd_possess(struct __psx_ctx * ctx, intptr_t * idx)
{
	int32_t		status;
	intptr_t	blkid;
	intptr_t	page;

	page  = *idx / __PSX_BITS_PER_PAGE;
	blkid = *idx % __PSX_BITS_PER_PAGE;

	if ((status = __ntapi->blt_possess(ctx->fd_blt_ctx[page],&blkid)))
		return 0;

	return &ctx->fd_slots[*idx];
}


int32_t __psx_fd_free (struct __psx_ctx * ctx, struct __fd * fd)
{
	int32_t		invalid;
	ptrdiff_t	blkid;
	struct __ofd *	ofd;

	/* invalidate */
	if ((invalid = at_locked_cas_32(&fd->invalid,0,1)))
		return NT_STATUS_INVALID_HANDLE;

	/* ref-counting */
	blkid = fd - ctx->fd_slots;
	ofd = &ctx->ofd_slots[fd->ofdidx];
	__psx_ofd_ref_dec(ctx,ofd);

	/* book-keeping */
	while (fd->refcnt);
	return __ntapi->blt_release(
		ctx->fd_blt_ctx[blkid / __PSX_BITS_PER_PAGE],
		blkid % __PSX_BITS_PER_PAGE);
}


struct __fd * __psx_fd_ref_inc(struct __psx_ctx * ctx, intptr_t fdidx)
{
	int32_t		invalid;
	struct __fd *	fd;

	if ((fdidx < 0) || (fdidx > ctx->fd_cap))
		return 0;

	fd = &ctx->fd_slots[fdidx];

	if (fd->invalid)
		return 0;
	else
		at_locked_inc_32(&fd->refcnt);

	if ((invalid = at_locked_or_32(&fd->invalid,0))) {
		at_locked_dec_32(&fd->refcnt);
		return 0;
	}

	return fd;
}


void __psx_fd_ref_dec(struct __psx_ctx * ctx, struct __fd * fd)
{
	at_locked_dec_32(&fd->refcnt);
}


struct __ofd * __psx_ofd_alloc(struct __psx_ctx * ctx, intptr_t * idx)
{
	int32_t		status;
	intptr_t	blkid;
	int		i,pages;
	struct __ofd *	ofd;

	pages = ctx->ofd_cap % __PSX_BITS_PER_PAGE
		? (ctx->ofd_cap / __PSX_BITS_PER_PAGE) +1
		: (ctx->ofd_cap / __PSX_BITS_PER_PAGE);

	for (i=0,status=-1; (i<pages) && (status); i++)
		status = __ntapi->blt_acquire(ctx->ofd_blt_ctx[i],&blkid);

	if (status)
		return 0;

	ofd = &ctx->ofd_slots[blkid];
	ofd->info.refcnt = 1;

	if (!ofd->info.hevent) {
		status = __ntapi->tt_create_private_event(
			&ofd->info.hevent,
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED);

		if (status) {
			__psx_ofd_free(ctx,ofd);
			return 0;
		}
	}

	*idx = (int32_t)blkid;
	return ofd;
}


void __psx_ofd_free (struct __psx_ctx * ctx, struct __ofd * ofd)
{
	ptrdiff_t blkid;
	void *    hevent;

	__iovtbl[ofd->info.fdtype].free(ofd);
	__iovtbl[ofd->info.fdtype].close(ofd->info.hfile);

	hevent = ofd->info.hevent;
	__ntapi->tt_aligned_block_memset(
		&ofd->info,0,sizeof(ofd->info));
	ofd->info.hevent = hevent;

	blkid = ofd - ctx->ofd_slots;
	__ntapi->blt_release(
		ctx->ofd_blt_ctx[blkid / __PSX_BITS_PER_PAGE],
		blkid % __PSX_BITS_PER_PAGE);
}


struct __ofd * __psx_ofd_ref_inc(struct __psx_ctx * ctx, intptr_t fdidx)
{
	struct __fd *	fd;
	struct __ofd *	ofd;

	if (!(fd = __psx_fd_ref_inc(ctx,fdidx)))
		return 0;

	ofd = &ctx->ofd_slots[fd->ofdidx];
	at_locked_inc_32(&ofd->info.refcnt);
	at_locked_dec_32(&fd->refcnt);

	return ofd;
}


void __psx_ofd_ref_dec(struct __psx_ctx * ctx, struct __ofd * ofd)
{
	if (at_locked_xsub_32(&ofd->info.refcnt,1) == 1)
		__psx_ofd_free(ctx,ofd);
}
