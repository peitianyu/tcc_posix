/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_process.h"
#include "psx_tlca.h"
#include "psx_ofd.h"
#include "psx_helper.h"
#include "psx_impl.h"

static int32_t __stdcall __create_process_return(nt_runtime_data_block * rtblock, int32_t status)
{
	__ntapi->zw_close(
		((nt_runtime_data *)rtblock)->hready);

	__ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&rtblock->addr,
		&rtblock->size,
		NT_MEM_RELEASE);

	return status;
}

int32_t __psx_create_foreign_process(
	struct __psx_tlca *	tlca,
	struct __ofd *		image,
	struct __ofd *		interpreter,
	const unsigned char *	optarg,
	const unsigned char *	path,
	const char **		argv,
	const char **		envp,
	uint32_t 		flags,
	const nt_alt_cid *	altcid,
	nt_process_info *	execve)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

int32_t __psx_create_native_process(
	struct __psx_tlca *	tlca,
	struct __ofd *		image,
	struct __ofd *		interpreter,
	const unsigned char *	optarg,
	const unsigned char *	path,
	const char **		argv,
	const char **		envp,
	uint32_t 		flags,
	const nt_alt_cid *	altcid,
	nt_process_info *	execve)
{
	int32_t				status;
	struct __psx_ctx *		ctx;
	nt_runtime_data_block		rtblock;
	nt_rtdata *			rdata;
	nt_create_process_params	params;
	nt_unicode_string *		imgname;
	uint32_t			written;
	wchar16_t **			pwarg;
	const char **			parg;
	wchar16_t *			wch;
	char **				rargv;
	char **				renvp;
	size_t				size;
	unsigned			pages;

	rtblock.addr		= 0;
	rtblock.size		= 0x10000;
	rtblock.remote_addr	= 0;
	rtblock.remote_size	= 0;
	rtblock.flags		= 0;

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			&rtblock.addr,0,
			&rtblock.size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE)))
		return status;

	__ntapi->tt_aligned_block_memset(
		rtblock.addr,0,rtblock.size);

	if ((status = __ntapi->zw_query_object(
			image->info.hfile,
			NT_OBJECT_NAME_INFORMATION,
			tlca->buffer,tlca->buflen,
			&written)))
		return __create_process_return(&rtblock,status);

	imgname = (nt_unicode_string *)tlca->buffer;
	rdata   = (nt_runtime_data *)rtblock.addr;
	envp	= envp ? envp : (const char **)rtdata->envp;
	ctx	= tlca->ctx;

	if ((status = __ntapi->tt_array_copy_utf8(
			&rdata->argc,
			argv,
			envp,
			0,0,0,
			rtblock.addr,
			rdata->buffer,
			rtblock.size - sizeof(*rdata),
			&rtblock.remote_size)))
		return __create_process_return(&rtblock,status);

	rdata->argv = (char **)&((nt_runtime_data *)0)->buffer;
	rdata->envp = rdata->argv + rdata->argc + 1;

	rdata->wargv = (wchar16_t **)(rdata->buffer + (rtblock.remote_size / sizeof(uintptr_t)) + 1);
	rdata->wenvp = rdata->wargv + rdata->argc + 1;

	rargv = rdata->argv + ((uintptr_t)rtblock.addr / sizeof(char *));
	renvp = rdata->envp + ((uintptr_t)rtblock.addr / sizeof(char *));

	for (parg=envp; parg && *parg; parg++)
		(void)0;

	pwarg = rdata->wenvp + (parg-envp) + 1;
	wch = (wchar16_t *)pwarg;

	if ((status = __ntapi->tt_array_convert_utf8_to_utf16(
			rargv,
			rdata->wargv,
			rdata,
			wch,
			rtblock.size - sizeof(wchar16_t)*(wch-(wchar16_t *)rdata->buffer),
			&rtblock.remote_size)))
		return __create_process_return(&rtblock,status);

	wch += rtblock.remote_size/sizeof(wchar16_t);

	if ((status = __ntapi->tt_array_convert_utf8_to_utf16(
			renvp,
			rdata->wenvp,
			rdata,
			wch,
			rtblock.size - sizeof(wchar16_t)*(wch-(wchar16_t *)rdata->buffer),
			&rtblock.remote_size)))
		return __create_process_return(&rtblock,status);

	rdata->wargv -= (uintptr_t)rtblock.addr / sizeof(wchar16_t *);
	rdata->wenvp -= (uintptr_t)rtblock.addr / sizeof(wchar16_t *);

	if ((1||hport_tty) && (status = __ntapi->tt_create_inheritable_event(
			&rdata->hready,
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED)))
		return __create_process_return(&rtblock,status);

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&rdata->cid_parent,
		(uintptr_t *)&rtdata->cid_self,
		sizeof(nt_cid));

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&rdata->alt_cid_parent,
		(uintptr_t *)&rtdata->alt_cid_parent,
		sizeof(nt_cid));

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&rdata->alt_cid_self,
		(uintptr_t *)altcid,
		sizeof(*altcid));

	rdata->srv_keys[0]	= rtdata->srv_keys[0];
	rdata->srv_keys[1]	= rtdata->srv_keys[1];
	rdata->srv_keys[2]	= rtdata->srv_keys[2];
	rdata->srv_keys[3]	= rtdata->srv_keys[3];
	rdata->srv_keys[4]	= rtdata->srv_keys[4];
	rdata->srv_keys[5]	= rtdata->srv_keys[5];

	if (flags & PSX_CREATE_PROCESS_CLONE_FD_TABLES) {
		/* fd tables */
		size = ctx->fd_cap * sizeof(struct __fd);

		if ((status = __psx_clone_primary_section(
				&rdata->uptr[PSX_RTDATA_UPTR_SECTION_FD],
				ctx->fd_slots,
				__PSX_OFD_CAP * sizeof(struct __fd),
				0,&size)))
			return __create_process_return(&rtblock,status);

		/* fd bitmaps */
		pages = ctx->fd_cap % __PSX_BITS_PER_PAGE
			? (ctx->fd_cap / __PSX_BITS_PER_PAGE) +1
			: (ctx->fd_cap / __PSX_BITS_PER_PAGE);

		size = pages * __PSX_VIRTUAL_PAGE_SIZE;

		if ((status = __psx_clone_primary_section(
				&rdata->uptr[PSX_RTDATA_UPTR_SECTION_FD_BITMAP],
				ctx->fd_bitmap_addr,
				__PSX_OFD_BITMAP_PAGES*__PSX_VIRTUAL_PAGE_SIZE,
				0,&size)))
			return __create_process_return(&rtblock,status);

		/* ofd tables */
		size = ctx->ofd_cap * sizeof(struct __ofd);

		if ((status = __psx_clone_primary_section(
				&rdata->uptr[PSX_RTDATA_UPTR_SECTION_OFD],
				ctx->ofd_slots,
				__PSX_OFD_CAP * sizeof(struct __fd),
				0,&size)))
			return __create_process_return(&rtblock,status);

		/* ofd bitmaps */
		pages = ctx->ofd_cap % __PSX_BITS_PER_PAGE
			? (ctx->ofd_cap / __PSX_BITS_PER_PAGE) +1
			: (ctx->ofd_cap / __PSX_BITS_PER_PAGE);

		size = pages * __PSX_VIRTUAL_PAGE_SIZE;

		if ((status = __psx_clone_primary_section(
				&rdata->uptr[PSX_RTDATA_UPTR_SECTION_OFD_BITMAP],
				ctx->ofd_bitmap_addr,
				__PSX_OFD_BITMAP_PAGES*__PSX_VIRTUAL_PAGE_SIZE,
				0,&size)))
			return __create_process_return(&rtblock,status);
	} else {
		rdata->uptr[PSX_RTDATA_UPTR_SECTION_FD]		= ctx->fd_sec;
		rdata->uptr[PSX_RTDATA_UPTR_SECTION_FD_BITMAP]	= ctx->fd_bitmap_sec;
		rdata->uptr[PSX_RTDATA_UPTR_SECTION_OFD]		= ctx->ofd_sec;
		rdata->uptr[PSX_RTDATA_UPTR_SECTION_OFD_BITMAP]	= ctx->ofd_bitmap_sec;
	}

	rdata->udat32[PSX_RTDATA_UDAT32_FD_CAP]  = ctx->fd_cap;
	rdata->udat32[PSX_RTDATA_UDAT32_OFD_CAP]	 = ctx->ofd_cap;

	/* you close it no you close it */
	rdata->uclose[PSX_RTDATA_UPTR_SECTION_FD]	  = rtctx.fd_sec;
	rdata->uclose[PSX_RTDATA_UPTR_SECTION_FD_BITMAP]	  = rtctx.fd_bitmap_sec;
	rdata->uclose[PSX_RTDATA_UPTR_SECTION_OFD]	  = rtctx.ofd_sec;
	rdata->uclose[PSX_RTDATA_UPTR_SECTION_OFD_BITMAP] = rtctx.ofd_bitmap_sec;

	rdata->hcwd	= ctx->cwd.hfile;
	rdata->hdrive	= ctx->cwd.hat;
	rdata->hroot	= ctx->root.hfile;
	rdata->hlog	= rtdata->hlog;



	/* hoppla */
	__ntapi->tt_aligned_block_memset(
		&params,0,sizeof(params));

	params.image_name		= imgname->buffer;
	params.rtblock			= &rtblock;
	params.creation_flags_process	= NT_PROCESS_CREATE_FLAGS_INHERIT_HANDLES;
	params.creation_flags_thread	= (hport_tty || (flags & PSX_CREATE_PROCESS_PID_TRANSFER))
						? NT_PROCESS_CREATE_FLAGS_CREATE_THREAD_SUSPENDED 
						: 0;

	if ((status = __ntapi->tt_create_native_process(&params)))
		return __create_process_return(&rtblock,status);




	/* finalize */
	if (execve) {
		execve->hprocess		= params.hprocess;
		execve->hthread		= params.hthread;
		execve->process_id	= params.cid.process_id;
		execve->thread_id	= params.cid.thread_id;
	}

	if (!(params.creation_flags_thread & NT_PROCESS_CREATE_FLAGS_CREATE_THREAD_SUSPENDED))
		return __create_process_return(&rtblock,NT_STATUS_SUCCESS);

	if (hport_tty && (status = __ntapi->tty_client_process_register(
			hport_tty,
			params.pbi.unique_process_id,
			0,0,0)))
		__ntapi->zw_terminate_process(
			params.hprocess,
			status);

	if (flags & PSX_CREATE_PROCESS_PID_TRANSFER)
		__ntapi->zw_close(rtdata->alt_cid_self.hentry);

	if ((status = __ntapi->zw_resume_thread(params.hthread,0)))
		__ntapi->zw_terminate_process(
			params.hprocess,
			status);

	else if (rdata->hready && (status = __ntapi->zw_wait_for_single_object(
			rdata->hready,
			NT_SYNC_NON_ALERTABLE,
			0)))
		__ntapi->zw_terminate_process(
			params.hprocess,
			status);

	{
		/* temporary cheating, work-in-progress */
		nt_pbi	pbi;

		/* parental control enabled? */
		if (rtdata->alt_cid_parent.pgid) {
			if ((status = __ntapi->zw_wait_for_single_object(
					params.hprocess,
					NT_SYNC_NON_ALERTABLE,
					0)))
				tlca->exitcode = status;

			if ((status = __ntapi->zw_query_information_process(
					params.hprocess,
					NT_PROCESS_BASIC_INFORMATION,
					&pbi,sizeof(pbi),0)))
				tlca->exitcode = status;

			else
				tlca->exitcode = pbi.exit_status;
		}
	}

	return __create_process_return(&rtblock,status);
}
