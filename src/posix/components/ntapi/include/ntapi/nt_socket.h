#ifndef _NT_SOCKET_H_
#define _NT_SOCKET_H_

/**
 *  socket api:
 *  -----------
 *  here we provide native libraries and applications
 *  with a minimal socket abstraction layer;
 *  if you are interested in posix socket semantics
 *  then this header is REALLY NOT what you
 *  are looking for, as the most portable scenario
 *  (psxscl+libc) neither requires nor allows for
 *  direct interaction with the below interfaces.
 *
 *  (additional information for the yet curious...)
 *
 *  client libraries and applications are responsible
 *  for the entire bookkeeping, which should be
 *  easy using the nt_socket structure.
 *
 *  kernel sockets are created using ZwCreateFile,
 *  and are then manipulated (bind, connect, send,
 *  recv, listen, accept, getsockname, etc.) using
 *  ZwDeviceIoControlFile.  accordingly, the main
 *  objective of the below interfaces is to provide
 *  thin wrappers around the above NT system calls,
 *  thereby releasing you from the tedious task
 *  of formatting control messages according to
 *  individual IOCTL codes, protocol types, or
 *  different versions of the operating system.
 *
 *  another noteworthy objective is the direct
 *  translation between posix socket semantics on the
 *  one hand, and the interfaces that this module
 *  provides on the other.  functions in this module
 *  normally take the same arguments as their
 *  posix equivalents, yet require a pointer
 *  to an nt_socket structure in place of an integer
 *  file descriptor (in the posix library, the task
 *  of converting an integer file descriptor to the
 *  above pointer is a trivial one).  for functions
 *  such as send() and recv(), the return value is
 *  the system's native NTSTATUS; the number of bytes
 *  sent or received is then returned in a separate
 *  argument, and can also be obtained, along with
 *  extended status, from the info member of the iosb
 *  argument.  last but not least, each function in
 *  this module accepts one or more optional arguments
 *  of questionable relevance:-)
**/

#include <psxtypes/psxtypes.h>
#include "nt_status.h"
#include "nt_object.h"
#include "nt_tty.h"

/* afd socket domains */
#define NT_AF_UNSPEC		(0x0000u)
#define NT_AF_UNIX		(0x0001u)
#define NT_AF_INET		(0x0002u)
#define NT_AF_IMPLINK		(0x0003u)
#define NT_AF_PUP		(0x0004u)
#define NT_AF_CHAOS		(0x0005u)
#define NT_AF_NS		(0x0006u)
#define NT_AF_IPX		(0x0006u) /* synonym */
#define NT_AF_ISO		(0x0007u)
#define NT_AF_OSI		(0x0007u) /* synonym */
#define NT_AF_ECMA		(0x0008u)
#define NT_AF_DATAKIT		(0x0009u)
#define NT_AF_CCITT		(0x000Au)
#define NT_AF_SNA		(0x000Bu)
#define NT_AF_DECnet		(0x000Cu)
#define NT_AF_DLI		(0x000Du)
#define NT_AF_LAT		(0x000Eu)
#define NT_AF_HYLINK		(0x000Fu)
#define NT_AF_APPLETALK		(0x0010u)
#define NT_AF_NETBIOS		(0x0011u)
#define NT_AF_VOICEVIEW		(0x0012u)
#define NT_AF_FIREFOX		(0x0013u)
#define NT_AF_UNKNOWN_1ST	(0x0014u)
#define NT_AF_BAN		(0x0015u)
#define NT_AF_ATM		(0x0016u)
#define NT_AF_INET6		(0x0017u)
#define NT_AF_CLUSTER		(0x0018u)
#define NT_AF_12844		(0x0019u)
#define NT_AF_IRDA		(0x001Au)
#define NT_AF_NETDES		(0x001Cu)
#define NT_AF_TCNPROCESS	(0x001Du)
#define NT_AF_TCNMESSAGE	(0x001Eu)
#define NT_AF_ICLFXBM		(0x001Fu)
#define NT_AF_BTH		(0x0020u)
#define NT_AF_LINK		(0x0021u)


/* afd socket types */
#define NT_SOCK_STREAM		(0x0001u)
#define NT_SOCK_DGRAM		(0x0002u)
#define NT_SOCK_RAW		(0x0003u)
#define NT_SOCK_RDM		(0x0004u)
#define NT_SOCK_SEQPACKET	(0x0005u)


/* afd socket protocols */
#define NT_IPPROTO_IP				0
#define NT_IPPROTO_HOPOPTS			0
#define NT_IPPROTO_ICMP				1
#define NT_IPPROTO_IGMP				2
#define NT_IPPROTO_GGP				3
#define NT_IPPROTO_IPV4				4
#define NT_IPPROTO_ST				5
#define NT_IPPROTO_TCP				6
#define NT_IPPROTO_CBT				7
#define NT_IPPROTO_EGP				8
#define NT_IPPROTO_IGP				9
#define NT_IPPROTO_PUP				12
#define NT_IPPROTO_UDP				17
#define NT_IPPROTO_IDP				22
#define NT_IPPROTO_RDP				27
#define NT_IPPROTO_IPV6				41
#define NT_IPPROTO_ROUTING			43
#define NT_IPPROTO_FRAGMENT			44
#define NT_IPPROTO_ESP				50
#define NT_IPPROTO_AH				51
#define NT_IPPROTO_ICMPV6			58
#define NT_IPPROTO_NONE				59
#define NT_IPPROTO_DSTOPTS			60
#define NT_IPPROTO_ND				77
#define NT_IPPROTO_ICLFXBM			78
#define NT_IPPROTO_PIM				103
#define NT_IPPROTO_PGM				113
#define NT_IPPROTO_L2TP				115
#define NT_IPPROTO_SCTP				132
#define NT_IPPROTO_RAW				255
#define NT_IPPROTO_MAX				256
#define NT_IPPROTO_RESERVED_RAW			257
#define NT_IPPROTO_RESERVED_IPSEC		258
#define NT_IPPROTO_RESERVED_IPSECOFFLOAD	259
#define NT_IPPROTO_RESERVED_WNV			260
#define NT_IPPROTO_RESERVED_MAX			261



/* tdi receive modes */
#define NT_TDI_RECEIVE_BROADCAST		(0x0004u)
#define NT_TDI_RECEIVE_MULTICAST		(0x0008u)
#define NT_TDI_RECEIVE_PARTIAL			(0x0010u)
#define NT_TDI_RECEIVE_NORMAL			(0x0020u)
#define NT_TDI_RECEIVE_EXPEDITED		(0x0040u)
#define NT_TDI_RECEIVE_PEEK			(0x0080u)
#define NT_TDI_RECEIVE_NO_RESPONSE_EXP		(0x0100u)
#define NT_TDI_RECEIVE_COPY_LOOKAHEAD		(0x0200u)
#define NT_TDI_RECEIVE_ENTIRE_MESSAGE		(0x0400u)
#define NT_TDI_RECEIVE_AT_DISPATCH_LEVEL	(0x0800u)
#define NT_TDI_RECEIVE_CONTROL_INFO		(0x1000u)
#define NT_TDI_RECEIVE_FORCE_INDICATION		(0x2000u)
#define NT_TDI_RECEIVE_NO_PUSH			(0x4000u)

/* tdi send modes */
#define NT_TDI_SEND_EXPEDITED			(0x0020u)
#define NT_TDI_SEND_PARTIAL			(0x0040u)
#define NT_TDI_SEND_NO_RESPONSE_EXPECTED	(0x0080u)
#define NT_TDI_SEND_NON_BLOCKING		(0x0100u)
#define NT_TDI_SEND_AND_DISCONNECT		(0x0200u)

/* tdi listen modes */
#define NT_TDI_QUERY_ACCEPT			(0x0001u)

/* tdi disconnect modes */
#define NT_TDI_DISCONNECT_WAIT			(0x0001u)
#define NT_TDI_DISCONNECT_ABORT			(0x0002u)
#define NT_TDI_DISCONNECT_RELEASE		(0x0004u)

/* afd ioctl codes */
#define NT_AFD_IOCTL_BIND			(0x12003u)
#define NT_AFD_IOCTL_CONNECT			(0x12007u)
#define NT_AFD_IOCTL_LISTEN			(0x1200Bu)
#define NT_AFD_IOCTL_ACCEPT			(0x1200Cu)
#define NT_AFD_IOCTL_DUPLICATE			(0x12010u)
#define NT_AFD_IOCTL_SEND			(0x1201Fu)
#define NT_AFD_IOCTL_UDP_SEND			(0x12023u)
#define NT_AFD_IOCTL_RECV			(0x12017u)
#define NT_AFD_IOCTL_DISCONNECT			(0x1202Bu)
#define NT_AFD_IOCTL_SELECT			(0x12024u)
#define NT_AFD_IOCTL_GET_SOCK_NAME		(0x1202Fu)
#define NT_AFD_IOCTL_GET_PEER_NAME		(0x1202Fu)
#define NT_AFD_IOCTL_SET_CONTEXT		(0x1202Fu)

/* afd socket shutdown bits */
#define NT_AFD_DISCONNECT_WR	(0x01u)
#define NT_AFD_DISCONNECT_RD	(0x02u)

/* socket portable shutdown options */
#define NT_SHUT_RD		(0x00u)
#define NT_SHUT_WR		(0x01u)
#define NT_SHUT_RDWR		(0x02u)

/* send, sendto, sendmsg: *nix flags and bit place-holders */
#define NT_MSG_OOB		(0x00000001u)
#define NT_MSG_PEEK		(0x00000002u)
#define NT_MSG_DONTROUTE	(0x00000004u)
#define NT_MSG_CTRUNC		(0x00000008u)
#define NT_MSG_PROXY		(0x00000010u)
#define NT_MSG_TRUNC		(0x00000020u)
#define NT_MSG_DONTWAIT		(0x00000040u)
#define NT_MSG_EOR		(0x00000080u)
#define NT_MSG_WAITALL		(0x00000100u)
#define NT_MSG_FIN		(0x00000200u)
#define NT_MSG_SYN		(0x00000400u)
#define NT_MSG_CONFIRM		(0x00000800u)
#define NT_MSG_RST		(0x00001000u)
#define NT_MSG_ERRQUEUE		(0x00002000u)
#define NT_MSG_NOSIGNAL		(0x00004000u)
#define NT_MSG_MORE		(0x00008000u)
#define NT_MSG_WAITFORONE	(0x00010000u)
#define NT_MSG_CMSG_CLOEXEC	(0x40000000u)


/* socket structures */
typedef struct _nt_socket {
	union {
		void *		hsocket;
		void *		hfile;
		void *		hpipe;
		nt_pty *	hpty;
	};

	void *			hevent;
	int32_t			refcnt;		/* reserved for client */
	uint16_t		fdtype;		/* reserved for client */
	uint16_t		sctype;		/* reserved for client */
	uint32_t		ctxid;		/* reserved for client */
	uint32_t		reserved;	/* reserved for client */
	uint32_t		psxflags;	/* reserved for client */
	uint32_t		ntflags;	/* sc_wait alert flags */
	nt_large_integer	timeout;
	int32_t			iostatus;
	int32_t			waitstatus;

	union {
		struct {
			uint16_t	domain;
			uint16_t	type;
			uint32_t	protocol;
		};

		void *	vfd;
		void *	vmount;
		void *	hpair;
		void *	dirctx;
	};
} nt_socket;


typedef struct _nt_scope_id{
	union {
		struct {
			uint32_t	zone  : 28;
			uint32_t	level : 4;
		};

		uint32_t	value;
	};
} nt_scope_id;


typedef struct _nt_sockaddr_in4 {
	uint16_t	sa_family;
	char		sa_data[14];
} nt_sockaddr_in4;


typedef struct _nt_sockaddr_in6 {
	uint16_t	sa_family;
	uint16_t	sa_port;
	uint32_t	sa_flow;
	unsigned char	sa_data[16];
	nt_scope_id	sa_scope;
} nt_sockaddr_in6;


typedef union _nt_sockaddr {
	nt_sockaddr_in4	sa_addr_in4;
	nt_sockaddr_in6 sa_addr_in6;
} nt_sockaddr;


typedef struct _nt_afd_buffer {
	size_t		length;
	char *		buffer;
} nt_afd_buffer;


typedef struct _nt_afd_listen_info {
	void *		unknown_1st;
	uint32_t	backlog;
	void *		unknown_2nd;
} nt_afd_listen_info;


typedef struct _nt_afd_accept_info {
	uint32_t	sequence;
	nt_sockaddr	addr;
	uint32_t	legacy[2];
} nt_afd_accept_info;


typedef struct _nt_afd_duplicate_info {
	uint32_t	unknown;
	uint32_t	sequence;
	void *		hsocket_dedicated;
} nt_afd_duplicate_info;


typedef struct _nt_afd_send_info {
	nt_afd_buffer *		afd_buffer_array;
	uint32_t		buffer_count;
	uint32_t		afd_flags;
	uint32_t		tdi_flags;
} nt_afd_send_info;


typedef struct _nt_afd_recv_info {
	nt_afd_buffer *		afd_buffer_array;
	uint32_t		buffer_count;
	uint32_t		afd_flags;
	uint32_t		tdi_flags;
} nt_afd_recv_info;


typedef struct _nt_afd_disconnect_info {
	uint32_t	shutdown_flags;
	uint32_t	unknown[3];
} nt_afd_disconnect_info;


/* socket functions */
typedef int32_t __cdecl ntapi_sc_socket(
	__out	nt_socket *		hssocket,
	__in	uint16_t		domain,
	__in	uint16_t		type,
	__in	uint32_t		protocol,
	__in	uint32_t		desired_access	__optional,
	__in	nt_sqos *		sqos		__optional,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_bind(
	__in	nt_socket *		hssocket,
	__in	const nt_sockaddr *	addr,
	__in	uintptr_t		addrlen,
	__in	uintptr_t		service_flags	__optional,
	__out	nt_sockaddr *		sockaddr	__optional,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_listen(
	__in	nt_socket *		hssocket,
	__in	uintptr_t		backlog,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_accept(
	__in	nt_socket *		hssock_listen,
	__in	nt_sockaddr *		addr,
	__in	uint16_t *		addrlen,
	__out	nt_socket *		hssock_dedicated,
	__in	uintptr_t		afdflags	__optional,
	__in	uintptr_t		tdiflags	__optional,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_connect(
	__in	nt_socket *		hssocket,
	__in	nt_sockaddr *		addr,
	__in	uintptr_t		addrlen,
	__in	uintptr_t		service_flags	__optional,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_send(
	__in	nt_socket *		hssocket,
	__in	const void *		buffer,
	__in	size_t			len,
	__out	ssize_t *		bytes_sent	__optional,
	__in	uintptr_t		afdflags	__optional,
	__in	uintptr_t		tdiflags	__optional,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_recv(
	__in	nt_socket *		hssocket,
	__in	const void *		buffer,
	__in	size_t			len,
	__out	ssize_t *		bytes_received	__optional,
	__in	uintptr_t		afdflags	__optional,
	__in	uintptr_t		tdiflags	__optional,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_shutdown(
	__in	nt_socket *		hssocket,
	__in	uintptr_t		psxhow,
	__in	uintptr_t		afdhow,
	__out	nt_io_status_block *	iosb	__optional);


typedef int32_t __cdecl ntapi_sc_getsockname(
	__in	nt_socket *		hssocket,
	__in	nt_sockaddr *		addr,
	__in	uint16_t *		addrlen,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_server_accept_connection(
	__in	nt_socket *		hssocket,
	__out	nt_afd_accept_info *	accept_info,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_server_duplicate_socket(
	__in	nt_socket *		hssock_listen,
	__in	nt_socket *		hssock_dedicated,
	__in	nt_afd_accept_info *	accept_info,
	__out	nt_io_status_block *	iosb		__optional);


typedef int32_t __cdecl ntapi_sc_wait(nt_socket * hssocket, nt_iosb * iosb, nt_timeout * timeout);

#endif
