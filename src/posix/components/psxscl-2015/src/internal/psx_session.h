/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_SESSION_H_
#define _PSX_SESSION_H_

#include <dalist/dalist.h>
#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_signal.h"

/********************************************************************/
/*                                                                  */
/*   thread         <--------->   thread                            */
/*   process        <--------->   process / thread group            */
/*   job            <--------->   process group                     */
/*   session(+)     <--------->   session                           */
/*                                                                  */
/*   (+) session management implemented at the subsystem level.     */
/*                                                                  */
/********************************************************************/

enum __process_type {
	PSX_PROCESS_PARENT,
	PSX_PROCESS_SELF,
	PSX_PROCESS_CHILD,
	PSX_PROCESS_GRANDCHILD,
	PSX_PROCESS_COUSIN,
	PSX_PROCESS_RELATIVE,
	PSX_PROCESS_PEER,
	PSX_PROCESS_FRIENDLY_NEIGHBOR,
	PSX_PROCESS_ABNOXIOUS_NEIGHBOR
};

#define PSX_WNOHANG	0x00000001
#define PSX_WUNTRACED	0x00000002

#define PSX_WSTOPPED	0x00000002
#define PSX_WEXITED	0x00000004
#define PSX_WCONTINUED	0x00000008
#define PSX_WNOWAIT	0x01000000

#define PSX_WNOTHREAD	0x20000000
#define PSX_WALL	0x40000000
#define PSX_WCLONE	0x80000000


struct __process_record {
	struct dalist_node_ex *		node;
	struct __process_record *	parent;
	void *				hprocess;
	void *				hport;
	nt_pbi				pbi;
	int32_t				type;
	int32_t				exitcode;
	int32_t				waitable;
	int32_t				key;
	nt_cid				cid;
	int32_t				tid;
	int32_t				pid;
	int32_t				pgid;
	int32_t				sid;
};

struct __process_record * __psx_get_process_record(
	struct dalist_ex *	clan,
	uintptr_t		syspid,
	uintptr_t		systid,
	int32_t			tid,
	int32_t			pid,
	int32_t			pgid);

struct __process_record * __psx_add_process_record(
	struct dalist_ex *	clan,
	void *			hprocess,
	void *			hport,
	int32_t			type,
	uintptr_t		syspid,
	uintptr_t		systid,
	int32_t			tid,
	int32_t			pid,
	int32_t			pgid,
	int32_t			sid);

int32_t __psx_session_fork(void);



static __inline__ void __psx_pid_lock_acquire(void)
{
	while (__ntapi->tt_sync_block_lock(
		&__psx.__sigfn_lock,
		0,0,0));
}

static __inline__ int32_t __psx_pid_lock_release(int32_t status)
{
	__ntapi->tt_sync_block_unlock(&__psx.__sigfn_lock);
	return status;
}

#endif
