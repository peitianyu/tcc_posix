/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_tty.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"
#include "ntapi_pty.h"

static int32_t __stdcall __ntapi_pty_open_close(
	nt_pty *	pty,
	nt_iosb *	iosb,
	int32_t		opcode)
{
	int32_t		status;
	nt_pty_fd_msg	msg;

	__ntapi->tt_aligned_block_memset(&msg,0,sizeof(msg));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg.data);
	msg.header.msg_size	= sizeof(msg);
	msg.data.ttyinfo.opcode	= opcode;

	msg.data.fdinfo.hpty	= pty->hpty;
	msg.data.fdinfo.access	= pty->access;
	msg.data.fdinfo.flags	= pty->flags;
	msg.data.fdinfo.share	= pty->share;
	msg.data.fdinfo.options	= pty->options;

	msg.data.fdinfo.luid.high = pty->luid.high;
	msg.data.fdinfo.luid.low  = pty->luid.low;

	__ntapi_tt_guid_copy(
		&msg.data.fdinfo.guid,
		&pty->guid);

	if ((status = __ntapi->zw_request_wait_reply_port(pty->hport,&msg,&msg)))
		return status;
	else if (msg.data.ttyinfo.status)
		return msg.data.ttyinfo.status;

	pty->hpty	  = msg.data.fdinfo.hpty;
	pty->section	  = msg.data.fdinfo.section;
	pty->section_size = msg.data.fdinfo.section_size;
	pty->luid.high	  = msg.data.fdinfo.luid.high;
	pty->luid.low	  = msg.data.fdinfo.luid.low;
	iosb->status	  = msg.data.ttyinfo.status;
	iosb->info	  = 0;

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __ntapi_pty_free(nt_pty * pty)
{
	void *	addr;
	size_t	size;

	/* unmap section */
	if (pty->section_addr)
		__ntapi->zw_unmap_view_of_section(
			NT_CURRENT_PROCESS_HANDLE,
			pty->section_addr);

	/* free control block */
	addr = pty->addr;
	size = pty->size;

	return __ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&addr,
		&size,
		NT_MEM_RELEASE);
}


static int32_t __fastcall __ntapi_pty_fail(nt_pty * pty,int32_t status)
{
	__ntapi_pty_free(pty);
	return status;
}


static int32_t __fastcall __ntapi_pty_alloc(nt_pty ** pty)
{
	int32_t		status;
	nt_pty *	ctx;
	size_t		ctx_size;

	/* allocate control block */
	ctx = 0;
	ctx_size = sizeof(nt_pty);

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&ctx,
			0,&ctx_size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE)))
		return status;

	/* init control block */
	__ntapi->tt_aligned_block_memset(
		ctx,0,ctx_size);

	ctx->addr = ctx;
	ctx->size = ctx_size;

	*pty = ctx;
	return NT_STATUS_SUCCESS;
}

static int32_t __ntapi_pty_connect(
	void *		hport,
	nt_pty *	ctx,
	nt_iosb *	iosb)
{
	int32_t status;

	ctx->hport = hport
		? hport
		: __ntapi_internals()->hport_tty_session;

	/* request */
	iosb = iosb ? iosb : &ctx->iosb;

	if ((status = __ntapi_pty_open_close(ctx,iosb,NT_TTY_PTY_OPEN)))
		return __ntapi_pty_fail(ctx,status);

	/* map section */
	if ((status = __ntapi->zw_map_view_of_section(
			ctx->section,
			NT_CURRENT_PROCESS_HANDLE,
			&ctx->section_addr,
			0,ctx->section_size,
			0,&ctx->section_size,
			NT_VIEW_UNMAP,0,
			NT_PAGE_READWRITE)))
		return __ntapi_pty_fail(ctx,status);

	/* assume conforming clients, config for single lock try */
	__ntapi->tt_sync_block_init(&ctx->sync[__PTY_READ],0,0,1,0,0);
	__ntapi->tt_sync_block_init(&ctx->sync[__PTY_WRITE],0,0,1,0,0);

	return NT_STATUS_SUCCESS;
}


int32_t	__stdcall __ntapi_pty_open(
	void *			hport,
	nt_pty **		pty,
	uint32_t		desired_access,
	nt_object_attributes*	obj_attr,
	nt_iosb *		iosb,
	uint32_t		share_access,
	uint32_t		open_options)
{
	int32_t			status;
	uint32_t		hash;
	nt_guid			guid;
	nt_uuid_str_utf16 *	guid_str;
	nt_pty *		ctx;

	if (!obj_attr || !obj_attr->obj_name || !obj_attr->obj_name->buffer)
		return NT_STATUS_INVALID_PARAMETER;

	if (obj_attr->obj_name->strlen != __DEVICE_PATH_PREFIX_LEN + sizeof(nt_guid_str_utf16))
		return NT_STATUS_OBJECT_PATH_INVALID;

	hash = __ntapi->tt_buffer_crc32(
		0,
		obj_attr->obj_name->buffer,
		__DEVICE_PATH_PREFIX_LEN);

	if (hash != __DEVICE_PATH_PREFIX_HASH)
		return NT_STATUS_OBJECT_PATH_INVALID;

	guid_str = (nt_uuid_str_utf16 *)
		((uintptr_t)obj_attr->obj_name->buffer + __DEVICE_PATH_PREFIX_LEN);

	if (__ntapi->tt_utf16_string_to_guid(guid_str,&guid))
		return NT_STATUS_OBJECT_NAME_INVALID;

	/* control block */
	if ((status = __ntapi_pty_alloc(&ctx)))
		return status;

	__ntapi_tt_guid_copy(
		&ctx->guid,
		&guid);

	ctx->access	= desired_access;
	ctx->flags	= obj_attr->obj_attr;
	ctx->share	= share_access;
	ctx->options	= open_options;

	/* pts */
	if (obj_attr->root_dir) {
		ctx->luid.high = ((nt_pty *)obj_attr->root_dir)->luid.high;
		ctx->luid.low  = ((nt_pty *)obj_attr->root_dir)->luid.low;
	}

	if ((status = __ntapi_pty_connect(hport,ctx,iosb)))
		return status;

	*pty = ctx;

	return NT_STATUS_SUCCESS;
}

int32_t	__stdcall __ntapi_pty_reopen(
	__in	void *			hport,
	__in	nt_pty *		pty)
{
	return __ntapi_pty_connect(hport,pty,0);
}

int32_t __stdcall __ntapi_pty_close(nt_pty * pty)
{
	if (!pty || (pty->addr != pty))
		return NT_STATUS_INVALID_PARAMETER;

	__ntapi_pty_open_close(
		pty,&pty->iosb,NT_TTY_PTY_CLOSE);

	return __ntapi_pty_free(pty);
}
