/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_sync.h>
#include <ntapi/nt_atomic.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

void __stdcall __ntapi_tt_sync_block_init(
	__in	nt_sync_block *	sync_block,
	__in	uint32_t	flags			__optional,
	__in	int32_t		srvtid			__optional,
	__in	int32_t		default_lock_tries	__optional,
	__in	int64_t		default_lock_wait	__optional,
	__in	void *		hsignal			__optional)
{
	__ntapi->tt_aligned_block_memset(
		sync_block,
		0,sizeof(*sync_block));

	sync_block->lock_tries = default_lock_tries
		? default_lock_tries
		: __NT_SYNC_BLOCK_LOCK_TRIES;

	sync_block->lock_wait.quad = default_lock_wait
		? default_lock_wait
		: (-1);

	sync_block->flags   = flags;
	sync_block->srvtid  = srvtid;
	sync_block->hsignal = hsignal;

	return;
}


int32_t __stdcall __ntapi_tt_sync_block_lock(
	__in	nt_sync_block *	sync_block,
	__in	int32_t		lock_tries	__optional,
	__in	int64_t		lock_wait	__optional,
	__in	uint32_t *	sig_flag	__optional)
{
	int32_t		status;
	int32_t		tid;
	intptr_t	lock;
	void *		hwait[2];
	nt_timeout	timeout;

	/* validation */
	if (sync_block->invalid)
		return NT_STATUS_INVALID_HANDLE;

	/* already owned? */
	tid = pe_get_current_thread_id();
	if (sync_block->tid == tid) return NT_STATUS_SUCCESS;

	/* yield to server? */
	if ((sync_block->flags & NT_SYNC_BLOCK_YIELD_TO_SERVER) && (tid != sync_block->srvtid)) {
		hwait[0] = sync_block->hserver;
		hwait[1] = sync_block->hsignal;

		/* signal support */
		if (sig_flag && *sig_flag)
			return NT_STATUS_ALERTED;

		/* wait */
		status = __ntapi->zw_wait_for_multiple_objects(
			2,
			hwait,
			NT_WAIT_ANY,
			NT_SYNC_NON_ALERTABLE,
			(nt_timeout *)0);

		/* signal support */
		if (sig_flag && *sig_flag)
			return NT_STATUS_ALERTED;
	}

	/* first try */
	lock = at_locked_cas_32(&sync_block->tid,0,tid);
	if (lock && !--lock_tries) return NT_STATUS_NOT_LOCKED;

	/* first-time contended case? */
	if (lock && !sync_block->hwait) {
		status = __ntapi->tt_create_inheritable_event(
			&hwait[0],
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED);

		if (status) return status;

		lock = at_locked_cas(
			(intptr_t *)&sync_block->hwait,
			0,(intptr_t)hwait);

		if (lock)
			__ntapi->zw_close(hwait);

		/* try again without a wait */
		lock = at_locked_cas_32(&sync_block->tid,0,tid);
	}

	/* contended case? */
	if (lock) {
		hwait[0] = sync_block->hwait;
		hwait[1] = sync_block->hsignal;

		lock_tries = lock_tries
			? lock_tries
			: sync_block->lock_tries;

		timeout.quad = lock_wait
			? lock_wait
			: sync_block->lock_wait.quad;

		for (; lock && lock_tries; lock_tries--) {
			/* signal support */
			if (sig_flag && *sig_flag)
				return NT_STATUS_ALERTED;

			/* wait */
			status = __ntapi->zw_wait_for_multiple_objects(
				2,
				&sync_block->hwait,
				NT_WAIT_ANY,
				NT_SYNC_NON_ALERTABLE,
				&timeout);

			/* check status */
			if ((status != NT_STATUS_TIMEOUT) && ((uint32_t)status >= NT_STATUS_WAIT_CAP))
				return status;

			/* signal support */
			if (sig_flag && *sig_flag)
				return NT_STATUS_ALERTED;

			/* try again */
			lock = at_locked_cas_32(&sync_block->tid,0,tid);
		};
	}

	if (lock) return NT_STATUS_NOT_LOCKED;

	/* shared section support */
	sync_block->pid = pe_get_current_process_id();

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_sync_block_server_lock(
	__in	nt_sync_block *	sync_block,
	__in	int32_t		lock_tries	__optional,
	__in	int64_t		lock_wait	__optional,
	__in	uint32_t *	sig_flag	__optional)
{
	int32_t status;

	/* validation */
	 if (sync_block->invalid)
		return NT_STATUS_INVALID_HANDLE;

	else if (sync_block->srvtid != pe_get_current_thread_id())
		return NT_STATUS_RESOURCE_NOT_OWNED;

	/* try once without yield request */
	status = __ntapi_tt_sync_block_lock(
		sync_block,
		1,
		lock_wait,
		sig_flag);

	if (status == NT_STATUS_SUCCESS)
		return status;

	/* hserver */
	if (!sync_block->hserver) {
		status = __ntapi->tt_create_inheritable_event(
			&sync_block->hserver,
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED);

		if (status) return status;
	} else {
		status = __ntapi->zw_reset_event(
			&sync_block->hserver,
			(int32_t *)0);

		if (status) return status;
	}

	/* yield request: set */
	sync_block->flags |= NT_SYNC_BLOCK_YIELD_TO_SERVER;

	/* try again */
	status = __ntapi_tt_sync_block_lock(
		sync_block,
		lock_tries,
		lock_wait,
		sig_flag);

	/* yield request: unset */
	sync_block->flags ^= NT_SYNC_BLOCK_YIELD_TO_SERVER;

	__ntapi->zw_set_event(
		sync_block->hserver,
		(int32_t *)0);

	/* (locking not guaranteed) */
	return status;
}


int32_t __stdcall __ntapi_tt_sync_block_unlock(
	__in	nt_sync_block *	sync_block)
{
	int64_t cmp;

	if (sync_block->invalid)
		return NT_STATUS_INVALID_HANDLE;

	cmp = (int64_t)(pe_get_current_process_id()) << 32;
	cmp += pe_get_current_thread_id();

	if (cmp != at_locked_cas_64(
			(int64_t *)&sync_block->tid,
			cmp,0))
		return NT_STATUS_RESOURCE_NOT_OWNED;

	return NT_STATUS_SUCCESS;
}


void __stdcall __ntapi_tt_sync_block_validate(
	__in	nt_sync_block *	sync_block)
{
	at_store_32(&sync_block->invalid,0);

	return;
}


int32_t __stdcall __ntapi_tt_sync_block_invalidate(
	__in	nt_sync_block *	sync_block)
{
	int32_t invalid;

	if (!sync_block)
		return NT_STATUS_INVALID_PARAMETER;

	invalid = at_locked_cas_32(
		&sync_block->invalid,
		0,
		1);

	if (invalid)
		return NT_STATUS_INVALID_HANDLE;

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_sync_block_discard(
	__in	nt_sync_block *	sync_block)
{
	if (!sync_block)
		return NT_STATUS_INVALID_PARAMETER;

	if (sync_block->hwait)
		__ntapi->zw_close(sync_block->hwait);

	if (sync_block->hserver)
		__ntapi->zw_close(sync_block->hserver);

	__ntapi->tt_aligned_block_memset(sync_block,-1,sizeof(*sync_block));

	return NT_STATUS_SUCCESS;
}
