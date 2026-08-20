#ifndef _PSX_SOCKET_H_
#define _PSX_SOCKET_H_

enum __psx_socket_type {
	PSX_SOCKET_UNIX,
	PSX_SOCKET_INET4,
	PSX_SOCKET_INET6,
	PSX_SOCKET_CAP
};

enum __sc_io_mode {
	__SENDTO,
	__RECVFROM
};

#define PSX_AF_UNSPEC       0
#define PSX_AF_LOCAL        1
#define PSX_AF_UNIX         PSX_AF_LOCAL
#define PSX_AF_FILE         PSX_AF_LOCAL
#define PSX_AF_INET         2
#define PSX_AF_AX25         3
#define PSX_AF_IPX          4
#define PSX_AF_APPLETALK    5
#define PSX_AF_NETROM       6
#define PSX_AF_BRIDGE       7
#define PSX_AF_ATMPVC       8
#define PSX_AF_X25          9
#define PSX_AF_INET6        10
#define PSX_AF_ROSE         11
#define PSX_AF_DECnet       12
#define PSX_AF_NETBEUI      13
#define PSX_AF_SECURITY     14
#define PSX_AF_KEY          15
#define PSX_AF_NETLINK      16
#define PSX_AF_ROUTE        PSX_AF_NETLINK
#define PSX_AF_PACKET       17
#define PSX_AF_ASH          18
#define PSX_AF_ECONET       19
#define PSX_AF_ATMSVC       20
#define PSX_AF_RDS          21
#define PSX_AF_SNA          22
#define PSX_AF_IRDA         23
#define PSX_AF_PPPOX        24
#define PSX_AF_WANPIPE      25
#define PSX_AF_LLC          26
#define PSX_AF_IB           27
#define PSX_AF_CAN          29
#define PSX_AF_TIPC         30
#define PSX_AF_BLUETOOTH    31
#define PSX_AF_IUCV         32
#define PSX_AF_RXRPC        33
#define PSX_AF_ISDN         34
#define PSX_AF_PHONET       35
#define PSX_AF_IEEE802154   36
#define PSX_AF_CAIF         37
#define PSX_AF_ALG          38
#define PSX_AF_NFC          39
#define PSX_AF_VSOCK        40
#define PSX_AF_MAX          41

typedef uint16_t sa_family_t;
typedef unsigned socklen_t;

struct __sockaddr {
	sa_family_t	sa_family;
	char		sa_data[];
};

struct __msghdr
{
        void *		msg_name;
        socklen_t	msg_namelen;
        struct iovec *	msg_iov;
        int		msg_iovlen;
	int		__labi;
        void *		msg_control;
        socklen_t	msg_controllen;
	socklen_t	msg_id;
        int		msg_flags;
};

#endif
