#ifndef _NT_DAEMON_H_
#define _NT_DAEMON_H_

/**
 *  daemon start-up routines
 *  -----------------------
 *  upon successful return from dsr_init(),
 *  an LPC-based daemon will have entered its
 *  main service loop in a dedicated thread,
 *  having already established an internal
 *  connection.
 *
 *  remarks
 *  -------
 *  1) in the larger-scale midipix project, three
 *     different daemons are initialized using the
 *     below api, namely the optional tty server,
 *     the optional virtual mount server, and the
 *     posix system call layer internal daemon.
 *
 *  2) the initialization routines make use of two
 *     events: a 'damon-is-ready' event, and an
 *     'internal-client-is-ready' event.  if these
 *     events are not provided by the application
 *     then they will be created by this library,
 *     in which case each of the two handles may
 *     optionally be saved to a caller-provided
 *     address.
 *
 *  3) the dedicated thread of a well-designed
 *     daemon should be able to use a stack that
 *     is relatively very small; to fine-tune
 *     the stack size of the daemon's dedicated
 *     thread, use the appropriate members of
 *     the nt_daemon_params structure.
**/

#include <psxtypes/psxtypes.h>
#include "nt_thread.h"
#include "nt_port.h"

/* NT_DSR_INIT_FLAGS */
#define	NT_DSR_INIT_DEFAULT		0x0000
#define	NT_DSR_INIT_GENERATE_KEYS	0x0001
#define	NT_DSR_INIT_FORMAT_KEYS		0x0002
#define NT_DSR_INIT_CLOSE_EVENTS	0x0004

/* daemon start: max wait: 156 seconds (twelve bonobo couples) */
#define NT_DSR_INIT_MAX_WAIT		156 * (-1) * 10 * 1000 * 1000

typedef int32_t __stdcall nt_daemon_routine(void * context);

typedef struct _nt_daemon_params {
	wchar16_t *		port_name;
	nt_port_keys *		port_keys;
	nt_port_name_keys *	port_name_keys;
	uintptr_t		port_msg_size;
	nt_daemon_routine *	daemon_once_routine;
	nt_daemon_routine *	daemon_loop_routine;
	void *			daemon_loop_context;
	void *			hport_daemon;
	void **			pport_daemon;
	void *			hport_internal_client;
	void **			pport_internal_client;
	void *			hevent_daemon_ready;
	void **			pevent_daemon_ready;
	void *			hevent_internal_client_ready;
	void **			pevent_internal_client_ready;
	void *			hthread_daemon_start;
	void *			hthread_daemon_loop;
	void *			hthread_internal_client;
	int32_t			exit_code_daemon_start;
	int32_t			exit_code_daemon_loop;
	int32_t			exit_code_internal_client;
	uint32_t		flags;
	size_t			stack_size_commit;
	size_t			stack_size_reserve;
	nt_user_stack *		stack_info;
} nt_daemon_params, nt_dsr_params;


typedef int32_t __stdcall ntapi_dsr_init(nt_daemon_params *);
typedef int32_t __stdcall ntapi_dsr_start(nt_daemon_params *);
typedef int32_t __stdcall ntapi_dsr_create_port(nt_daemon_params *);
typedef int32_t __stdcall ntapi_dsr_connect_internal_client(nt_daemon_params *);
typedef int32_t __stdcall ntapi_dsr_internal_client_connect(nt_daemon_params *);
typedef int32_t __stdcall ntapi_dsr_loop(void *);

#endif
