/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <dalist/dalist.h>
#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_init.h"
#include "psx_session.h"
#include "psx_debug.h"
#include "psx_acl.h"
#include "psx.h"

/*~~~~~~~~~~~~~~~~~*/
/* way overboard  */
/*~~~~~~~~~~~~~~~*/

static uintptr_t __pid_key = 0;

struct __process_record * __psx_get_process_record(
	struct dalist_ex *	clan,
	uintptr_t		syspid,
	uintptr_t		systid,
	int32_t			tid,
	int32_t			pid,
	int32_t			pgid)
{
	int32_t				status;
	struct dalist_node_ex *		node;
	struct __process_record *	rec;
	void *				hprocess;
	nt_cid				cid;
	nt_oa				oa;

	__psx_pid_lock_acquire();

	node = (struct dalist_node_ex *)clan->head;

	for (; node; node=node->next) {
		rec = (struct __process_record *)&node->dblock;

		if ((syspid && (rec->cid.process_id == syspid))
				|| (systid && (rec->cid.thread_id == systid))
				|| (tid && (rec->tid == tid))
				|| (pid && (rec->pid == pid))
				|| (pgid && (rec->pgid == pgid))) {

			__psx_pid_lock_release(0);
			return rec;
		}	
	}

	if (!syspid)
		return 0;

	cid.process_id = syspid;
	cid.thread_id  = systid;

	oa.len		= sizeof(oa);
	oa.root_dir	= 0;
	oa.obj_name	= 0;
	oa.obj_attr	= 0;
	oa.sec_desc	= __PSX_DEF_SEC_DESC;
	oa.sec_qos	= __PSX_DEF_SEC_QOS;


	status = __ntapi->zw_open_process(
		&hprocess,
		NT_PROCESS_TERMINATE|NT_PROCESS_SYNCHRONIZE,
		&oa,&cid);

	if (status == NT_STATUS_ACCESS_DENIED)
		status = __ntapi->zw_open_process(
			&hprocess,
			NT_PROCESS_SYNCHRONIZE,
			&oa,&cid);

	if (status)
		__psx_pid_lock_release(status);
	else
		rec = __psx_add_process_record(
			clan,
			hprocess,0,
			PSX_PROCESS_FRIENDLY_NEIGHBOR,
			cid.process_id,
			cid.thread_id,
			0,0,0,0);

	return status ? 0 : rec;
}

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
	int32_t			sid)
{
	int32_t				status;
	struct dalist_node_ex *		node;
	struct __process_record *	rec;

	__psx_pid_lock_acquire();

	if ((status = dalist_get_free_node(clan,(void **)&node))) {
		__psx_pid_lock_release(status);
		return 0;
	}

	rec = (struct __process_record *)&node->dblock;
	node->key = ++__pid_key;

	rec->node	= node;
	rec->parent	= 0;
	rec->hprocess	= hprocess;
	rec->hport	= hport;
	rec->type	= type;
	rec->waitable	= !!hprocess;
	rec->exitcode	= -1;
	rec->tid		= tid;
	rec->pid		= pid;
	rec->pgid	= pgid;
	rec->sid		= sid;

	rec->cid.process_id = syspid;
	rec->cid.thread_id  = systid;

	if (dalist_insert_node_by_key(clan,node))
		/* debug print removed */;

	__psx_pid_lock_release(0);

	return rec;
}

int32_t __psx_remove_process_record(struct dalist_ex * clan,struct __process_record * rec)
{
	__psx_pid_lock_acquire();
	return dalist_discard_node(clan,rec->node);
	__psx_pid_lock_release(0);
}

int32_t __psx_session_fork(void)
{
	/* todo: optimize: discard all nodes */
	return NT_STATUS_SUCCESS;
}

int32_t __psx_axe(struct dalist_ex * clan, pid_t pid, int32_t exitcode)
{
	struct dalist_node_ex *		node;
	struct __process_record *	rec;
	int32_t				axepgid;
	int32_t				status;

	__psx_pid_lock_acquire();

	if ((pid == -1) && (!hport_tty))
		pid = 0;
	else if (pid == -1)
		return __psx_pid_lock_release(NT_STATUS_ACCESS_DENIED);

	if ((pid > 0) && (rec = __psx_get_process_record(clan,0,0,0,pid,0))) {
		if ((status = __ntapi->zw_terminate_process(rec->hprocess,exitcode)))
			return __psx_pid_lock_release(status);
		else {
			rec->exitcode = exitcode;
			return __psx_pid_lock_release(NT_STATUS_SUCCESS);
		}
	}

	axepgid = (pid == 0)
		? rtdata->alt_cid_self.pgid
		: pid;

	node = (struct dalist_node_ex *)clan->head;

	for (; node; node=node->next) {
		rec = (struct __process_record *)&node->dblock;

		if (rec->pgid == axepgid)
			__ntapi->zw_terminate_process(
				rec->hprocess,
				exitcode);
	}

	if (axepgid == rtdata->alt_cid_self.pgid)
		__ntapi->zw_terminate_process(
			rtdata->hprocess_self,
			exitcode);

	return __psx_pid_lock_release(NT_STATUS_INTERNAL_ERROR);
}
