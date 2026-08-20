/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_init.h"
#include "psx.h"

static int __psx_init_target_object(nt_alt_cid * altcid)
{
	int32_t			status;
	nt_oa			oa;
	nt_unicode_string	name;
	nt_port_name		objname;
	nt_port_attr		attr = {NT_PROCESS_GUID_PIDANY};
	wchar16_t		prefix[6] = NT_PROCESS_OBJDIR_PREFIX_PIDANY;
	nt_sqos			sqos = {
					sizeof(sqos),
					NT_SECURITY_IMPERSONATION,
					NT_SECURITY_TRACKING_DYNAMIC,
					1};

	if (altcid->htarget)
		return NT_STATUS_SUCCESS;

	if ((status = __ntapi->tt_port_generate_keys(&attr.keys)))
		return status;

	__ntapi->tt_port_name_from_attributes(&objname,&attr);
	__ntapi->tt_memcpy_utf16(objname.svc_prefix,prefix,sizeof(prefix));

	name.strlen = (uint16_t)(size_t)&((nt_port_name *)0)->null_termination;
	name.maxlen = 0;
	name.buffer = objname.base_named_objects;

	oa.len		= sizeof(oa);
	oa.root_dir	= 0;
	oa.obj_name	= &name;
	oa.obj_attr	= NT_OBJ_INHERIT;
	oa.sec_desc	= 0;
	oa.sec_qos	= &sqos;

	return __ntapi->zw_create_event(
		&altcid->htarget,
		NT_EVENT_ALL_ACCESS,
		&oa,
		NT_NOTIFICATION_EVENT,
		NT_EVENT_NOT_SIGNALED);
}


static int __psx_init_pgrp_object_directory(nt_alt_cid * altcid)
{
	nt_guid			guid = NT_PROCESS_GUID_PIDANY;
	wchar16_t		prefix[6] = NT_PROCESS_OBJDIR_PREFIX_NTPGRP;

	if (altcid->hpgrp)
		return NT_STATUS_SUCCESS;

	altcid->tid  = (int32_t)rtdata->cid_self.thread_id;
	altcid->pid  = (int32_t)rtdata->cid_self.process_id;
	altcid->pgid = (int32_t)rtdata->cid_parent.process_id;
	altcid->sid  = (int32_t)rtdata->cid_parent.process_id;

	return __ntapi->tt_create_keyed_object_directory(
		&altcid->hpgrp,
		NT_DIRECTORY_ALL_ACCESS,
		prefix,&guid,altcid->pgid);
}


int __psx_init_pgid(void)
{
	int32_t			status;
	nt_alt_cid *		altcid;
	nt_tty_session_info	sessioninfo;

	altcid = &rtdata->alt_cid_self;

	if ((status = __psx_init_target_object(altcid)))
		return status;

	if ((status = __psx_init_pgrp_object_directory(altcid)))
		return status;

	altcid->hsession = hport_tty;
	altcid->hdaemon  = hport_daemon;

	if ((status = __ntapi->tt_create_keyed_object_directory_entry(
			&altcid->hentry,
			NT_SYMBOLIC_LINK_ALL_ACCESS,
			altcid->hpgrp,
			altcid->hdaemon,0,
			altcid->pid)))
		return status;

	if (hport_tty) {
		if (altcid->pid == altcid->pgid) {
			sessioninfo.pid		= altcid->pid;
			sessioninfo.pgid	= altcid->pgid;
			sessioninfo.sid		= altcid->sid;
			sessioninfo.reserved	= 0;
		} else {
			sessioninfo.pid		= altcid->pid;
			sessioninfo.pgid	= 0;
			sessioninfo.sid		= 0;
			sessioninfo.reserved	= 0;
		}

		if ((status = __ntapi->tty_client_session_set(hport_tty,&sessioninfo)))
			return status;
	}

	return NT_STATUS_SUCCESS;
}
