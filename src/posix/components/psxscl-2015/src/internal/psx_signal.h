/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_SIGNAL_H_
#define _PSX_SIGNAL_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_ctx.h"
#include "psx_impl.h"

#define __PSX_ALERTABLE_IO 2

static __inline__ intptr_t __psx_sig_prolog(struct __psx_tlca * tlca)
{
	intptr_t ret;

	/* disallow thread suspension */
	at_locked_inc_32(&tlca->sig_tlock);

	/* record number of pending apc and daemon signals */
	ret = tlca->sig_count;

	/* process pending signals */
	if (ret)
		__ntapi->zw_delay_execution(&tlca->cfalert,0);

	/* return to syscall */
	return ret;
}

static __inline__ intptr_t __psx_sig_epilog(struct __psx_tlca * tlca,intptr_t ret, int32_t status)
{
	/* process pending signals */
	if (tlca->sig_count)
		__ntapi->zw_delay_execution(&tlca->cfalert,0);

	/* native status */
	tlca->ntstatus = status;

	/* allow thread suspension */
	at_locked_dec_32(&tlca->sig_tlock);

	/* syscall return value */
	return ret;
}

static __inline__ intptr_t __psx_io_prolog(struct __psx_tlca * tlca)
{
	intptr_t ret;

	/* mark thread as alertable */
	at_store_32(&tlca->sig_tlock,__PSX_ALERTABLE_IO);

	/* record number of pending apc and daemon signals */
	ret = tlca->sig_count;

	/* process pending signals */
	if (ret)
		__ntapi->zw_delay_execution(&tlca->cfalert,0);

	/* return to syscall */
	return ret;
}

static __inline__ intptr_t __psx_io_epilog(struct __psx_tlca * tlca,intptr_t ret, int32_t status)
{
	/* process pending signals */
	if (tlca->sig_count)
		__ntapi->zw_delay_execution(&tlca->cfalert,0);

	/* native status */
	tlca->ntstatus = status;

	/* allow thread suspension */
	at_store_32(&tlca->sig_tlock,0);

	/* syscall return value */
	return ret;
}

#endif
