/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_init.h"
#include "psx.h"

static int __psx_tty_join_session(void)
{
	int32_t		status;
	nt_port_attr	port_attr;
	nt_guid		pts_guid = TTY_PTS_GUID;

status=0;
while (status);

	__ntapi->tt_aligned_block_memset(
		&port_attr,0,sizeof(port_attr));

	port_attr.type		= NT_PORT_TYPE_SUBSYSTEM;
	port_attr.subtype	= NT_PORT_SUBTYPE_DEFAULT;

	port_attr.keys.key[0]	= rtdata->srv_keys[0];
	port_attr.keys.key[1]	= rtdata->srv_keys[1];
	port_attr.keys.key[2]	= rtdata->srv_keys[2];
	port_attr.keys.key[3]	= rtdata->srv_keys[3];
	port_attr.keys.key[4]	= rtdata->srv_keys[4];
	port_attr.keys.key[5]	= rtdata->srv_keys[5];

	__ntapi->tt_port_guid_from_type(
		&port_attr.guid,
		port_attr.type,
		port_attr.subtype);

	if ((status = __ntapi->tty_join_session(
			&hport_tty,0,
			&port_attr,
			NT_TTY_SESSION_PRIMARY)))
		return status;

	return __ntapi->tty_request_peer(
		hport_tty,PSX_DAEMON_TTYSIGNAL,0,&pts_guid,&daemon_attr);
}

int __psx_init_tty(void)
{
	int32_t status;
	nt_guid pts_guid = TTY_PTS_GUID;

	if (rtdata->srv_keys[0])
		return __psx_tty_join_session();

	else if (!(0 || (rtdata->ctx_options & __PSXOPT_POSIX)))
		return NT_STATUS_SUCCESS;

	if ((status = __ntapi->tty_create_session(
			&hport_tty,0,NT_TTY_SESSION_PRIMARY,0,0)))
		return status;

	return __ntapi->tty_request_peer(
		hport_tty,PSX_DAEMON_TTYSIGNAL,0,&pts_guid,&daemon_attr);
}
