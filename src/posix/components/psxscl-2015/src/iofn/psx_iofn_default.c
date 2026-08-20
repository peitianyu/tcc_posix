/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_path.h"
#include "psx_iovtbl.h"
#include "psx_tlca.h"
#include "psx_impl.h"

int32_t __fastcall __psx_iofn_default_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t __fastcall __psx_iofn_default_prolog(struct __psx_tlca * tlca, struct __ofd * ctxofd, struct __ofd ** ofd)
{
	int32_t status;

	if (!(at_locked_cas_32(&ctxofd->sync.tid,0,pe_get_current_thread_id()))) {
		*ofd = ctxofd;

		return ctxofd->info.hevent
			? NT_STATUS_SUCCESS
			: __ntapi->tt_create_private_event(
				&ctxofd->info.hevent,
				NT_NOTIFICATION_EVENT,
				NT_EVENT_NOT_SIGNALED);
	}

	if (!tlca->hevent && (status = __ntapi->tt_create_private_event(
			&tlca->hevent,
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED)))
		return status;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&tlca->ofd.info,
		(uintptr_t *)&ctxofd->info,
		sizeof(tlca->ofd.info));

	tlca->ofd.info.hevent = tlca->hevent;
	*ofd = &tlca->ofd;

	return NT_STATUS_SUCCESS;
}

void __fastcall __psx_iofn_default_epilog(struct __ofd * ctxofd, struct __ofd * ofd)
{
	if (ofd == ctxofd)
		at_store_32(&ctxofd->sync.tid,0);
}

int32_t __fastcall __psx_iofn_default_alloc(struct __path_info * path_info)
{
	return NT_STATUS_SUCCESS;
}

int32_t __fastcall __psx_iofn_default_free(struct __ofd * ofd)
{
	return NT_STATUS_SUCCESS;
}

int32_t __fastcall __psx_iofn_default_stat(struct __psx_tlca * tlca, struct __ofd * ofd, struct __stat * xstat)
{
	return NT_STATUS_NOT_SUPPORTED;
}

int32_t __fastcall __psx_iofn_default_unlink(struct __ofd * ofd, uint32_t flags)
{
	return NT_STATUS_ACCESS_DENIED;
}

int32_t __fastcall __psx_iofn_default_poll(struct __pollofd * pollofd, uint32_t flags)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t __fastcall __psx_iofn_default_peek(struct __ofd * ofd)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t __fastcall __psx_iofn_default_fsync(struct __ofd * ofd, nt_iosb * iosb)
{
	return NT_STATUS_SUCCESS;
}

int32_t __fastcall __psx_iofn_default_notify(struct __ofd * nofd, struct __ofd * fofd, int action, uint32_t mask)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t __stdcall __psx_iofn_default_create(
	void **			hfile,
	uint32_t		desired_access,
	nt_object_attributes *	obj_attr,
	nt_io_status_block *	io_status_block,
	nt_large_integer *	allocation_size,
	uint32_t		file_attr,
	uint32_t		share_access,
	uint32_t		create_disposition,
	uint32_t		create_options,
	void *			ea_buffer,
	uint32_t		ea_length)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_open(
	void **			hfile,
	uint32_t		desired_access,
	nt_object_attributes *	obj_attr,
	nt_io_status_block *	io_status_block,
	uint32_t		share_access,
	uint32_t		open_options)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t __stdcall __psx_iofn_default_close(void * handle)
{
	return NT_STATUS_SUCCESS;
}

int32_t	__stdcall __psx_iofn_default_read(
	void *			hfile,
	void *			hevent,
	nt_io_apc_routine *	apc_routine,
	void *			apc_context,
	nt_io_status_block *	io_status_block,
	void *			buffer,
	uint32_t		bytes_to_read,
	nt_large_integer *	byte_offset,
	uint32_t *		key)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t __stdcall __psx_iofn_default_write(
	void *			hfile,
	void *			hevent,
	nt_io_apc_routine *	apc_routine,
	void * 			apc_context,
	nt_io_status_block * 	io_status_block,
	void * 			buffer,
	uint32_t		bytes_sent,
	nt_large_integer *	byte_offset,
	uint32_t *		key)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_fcntl(
	void *				hfile,
	void *				hevent,
	nt_io_apc_routine *		apc_routine,
	void *				apc_context,
	nt_io_status_block *		io_status_block,
	uint32_t			fs_control_code,
	void *				input_buffer,
	uint32_t			input_buffer_length,
	void *				output_buffer,
	uint32_t			output_buffer_length)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_ioctl(
	void *				hfile,
	void *				hevent,
	nt_io_apc_routine *		apc_routine,
	void *				apc_context,
	nt_io_status_block *		io_status_block,
	uint32_t			io_control_code,
	void *				input_buffer,
	uint32_t			input_buffer_length,
	void *				output_buffer,
	uint32_t			output_buffer_length)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_lock(
	void *				hfile,
	void *				hevent,
	nt_io_apc_routine *		apc_routine,
	void *				apc_context,
	nt_io_status_block *		io_status_block,
	nt_large_integer *		lock_offset,
	nt_large_integer *		lock_length,
	uint32_t *			key,
	unsigned char			fail_immediately,
	unsigned char			exclusive_lock)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_unlock(
	void *				hfile,
	nt_io_status_block *		io_status_block,
	nt_large_integer *		lock_offset,
	nt_large_integer *		lock_length,
	uint32_t *			key)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_query(
	void *			hfile,
	nt_io_status_block *	io_status_block,
	void *			file_info,
	uint32_t		file_info_length,
	nt_file_info_class	file_info_class)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_set(
	void *			hfile,
	nt_io_status_block *	io_status_block,
	void *			file_info,
	uint32_t		file_info_length,
	nt_file_info_class	file_info_class)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_cancel(
	void *			hfile,
	nt_io_status_block *	io_status_block)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_remove(
	nt_object_attributes *	obj_attr)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t	__stdcall __psx_iofn_default_getdents(
	void *			hfile,
	void *			hevent,
	nt_io_apc_routine *	apc_routine,
	void *			apc_context,
	nt_io_status_block *	io_status_block,
	void *			file_info,
	uint32_t		file_info_length,
	nt_file_info_class	file_info_class,
	unsigned char		return_single_entry,
	nt_unicode_string *	file_name,
	unsigned char		restart_scan)
{
	return NT_STATUS_NOT_A_DIRECTORY;
}

int32_t	__stdcall __psx_iofn_default_getvents(
	void *			hfile,
	void *			hevent,
	nt_io_apc_routine *	apc_routine,
	void *			apc_context,
	nt_io_status_block *	io_status_block,
	void *			file_info,
	uint32_t		file_info_length,
	nt_file_info_class	file_info_class,
	unsigned char		return_single_entry,
	nt_unicode_string *	file_name,
	unsigned char		restart_scan)
{
	return NT_STATUS_NOT_A_DIRECTORY;
}

int32_t __stdcall __psx_iofn_default_open_logical_parent(
	void **		hparent,
	void *		hdir,
	uintptr_t *	buffer,
	uint32_t	buffer_size,
	uint32_t	desired_access,
	uint32_t	open_options,
	int32_t *	type)
{
	return NT_STATUS_BAD_FILE_TYPE;
}

int32_t __stdcall __psx_iofn_default_open_physical_parent(
	void **		hparent,
	void *		hdir,
	uintptr_t *	buffer,
	uint32_t	buffer_size,
	uint32_t	desired_access,
	uint32_t	open_options,
	int32_t *	type)
{
	return NT_STATUS_BAD_FILE_TYPE;
}
