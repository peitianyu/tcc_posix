#ifndef _NT_TERMIOS_H_
#define _NT_TERMIOS_H_

#include <psxtypes/psxtypes.h>

/* tty friendly guids */
#define TTY_PTM_GUID	{0x21b51c45,0x3388,0x4dd9,{0x82,0x9a,0x5b,0x67,0x4e,0x3e,0x31,0x55}}
#define TTY_PTS_GUID	{0xa038ed3e,0x7bcc,0x4a53,{0xb2,0x94,0x01,0xdf,0x87,0xf6,0x94,0x70}}
#define TTY_DBG_GUID	{0x5ad03536,0xde3c,0x451a,{0xa4,0x32,0xf6,0xfd,0x95,0x97,0x5c,0x52}}

/* cc_chars */
#define	TTY_NCCS	32

#define TTY_VINTR	0x00
#define TTY_VQUIT	0x01
#define TTY_VERASE	0x02
#define TTY_VKILL	0x03
#define TTY_VEOF	0x04
#define TTY_VTIME	0x05
#define TTY_VMIN	0x06
#define TTY_VSWTC	0x07
#define TTY_VSTART	0x08
#define TTY_VSTOP	0x09
#define TTY_VSUSP	0x0a
#define TTY_VEOL	0x0b
#define TTY_VREPRINT	0x0c
#define TTY_VDISCARD	0x0d
#define TTY_VWERASE	0x0e
#define TTY_VLNEXT	0x0f
#define TTY_VEOL2	0x10

/* c_iflag bits */
#define TTY_IGNBRK	0000001
#define TTY_BRKINT	0000002
#define TTY_IGNPAR	0000004
#define TTY_PARMRK	0000010
#define TTY_INPCK	0000020
#define TTY_ISTRIP	0000040
#define TTY_INLCR	0000100
#define TTY_IGNCR	0000200
#define TTY_ICRNL	0000400
#define TTY_IUCLC	0001000
#define TTY_IXON	0002000
#define TTY_IXANY	0004000
#define TTY_IXOFF	0010000
#define TTY_IMAXBEL	0020000
#define TTY_IUTF8	0040000

/* c_oflag bits */
#define TTY_OPOST	0000001
#define TTY_OLCUC	0000002
#define TTY_ONLCR	0000004
#define TTY_OCRNL	0000010
#define TTY_ONOCR	0000020
#define TTY_ONLRET	0000040
#define TTY_OFILL	0000100
#define TTY_OFDEL	0000200
#define TTY_NLDLY	0000400
#define TTY_NL0		0000000
#define TTY_NL1		0000400
#define TTY_CRDLY	0003000
#define TTY_CR0		0000000
#define TTY_CR1		0001000
#define TTY_CR2		0002000
#define TTY_CR3		0003000
#define TTY_TABDLY	0014000
#define TTY_TAB0	0000000
#define TTY_TAB1	0004000
#define TTY_TAB2	0010000
#define TTY_TAB3	0014000
#define TTY_BSDLY	0020000
#define TTY_BS0		0000000
#define TTY_BS1		0020000
#define TTY_FFDLY	0100000
#define TTY_FF0		0000000
#define TTY_FF1		0100000

#define TTY_VTDLY	0040000
#define TTY_VT0		0000000
#define TTY_VT1		0040000

/* c_lflag bits */
#define TTY_ISIG	0000001
#define TTY_ICANON	0000002
#define TTY_ECHO	0000010
#define TTY_ECHOE	0000020
#define TTY_ECHOK	0000040
#define TTY_ECHONL	0000100
#define TTY_NOFLSH	0000200
#define TTY_TOSTOP	0000400
#define TTY_IEXTEN	0100000

#define TTY_ECHOCTL	0001000
#define TTY_ECHOPRT	0002000
#define TTY_ECHOKE	0004000
#define TTY_FLUSHO	0010000
#define TTY_PENDIN	0040000

/* c_cflag bits */
#define TTY_CBAUD	0010017
#define TTY_CSIZE	0000060
#define TTY_CS5		0000000
#define TTY_CS6		0000020
#define TTY_CS7		0000040
#define TTY_CS8		0000060
#define TTY_CSTOPB	0000100
#define TTY_CREAD	0000200
#define TTY_PARENB	0000400
#define TTY_PARODD	0001000
#define TTY_HUPCL	0002000
#define TTY_CLOCAL	0004000

/* control flow */
#define TTY_TCOOFF	0
#define TTY_TCOON	1
#define TTY_TCIOFF	2
#define TTY_TCION	3

/* flush */
#define TTY_TCIFLUSH	0
#define TTY_TCOFLUSH	1
#define TTY_TCIOFLUSH	2

/* tty ioctl */
#define TTY_TCSANOW	0
#define TTY_TCSADRAIN	1
#define TTY_TCSAFLUSH	2

/* tty ioctl codes */
#define TTY_TCGETS		0x5401
#define TTY_TCSETS		0x5402
#define TTY_TCSETSW		0x5403
#define TTY_TCSETSF		0x5404
#define TTY_TCGETA		0x5405
#define TTY_TCSETA		0x5406
#define TTY_TCSETAW		0x5407
#define TTY_TCSETAF		0x5408
#define TTY_TCSBRK		0x5409
#define TTY_TCXONC		0x540A
#define TTY_TCFLSH		0x540B
#define TTY_TIOCEXCL		0x540C
#define TTY_TIOCNXCL		0x540D
#define TTY_TIOCSCTTY		0x540E
#define TTY_TIOCGPGRP		0x540F
#define TTY_TIOCSPGRP		0x5410
#define TTY_TIOCOUTQ		0x5411
#define TTY_TIOCSTI		0x5412
#define TTY_TIOCGWINSZ		0x5413
#define TTY_TIOCSWINSZ		0x5414
#define TTY_TIOCMGET		0x5415
#define TTY_TIOCMBIS		0x5416
#define TTY_TIOCMBIC		0x5417
#define TTY_TIOCMSET		0x5418
#define TTY_TIOCGSOFTCAR	0x5419
#define TTY_TIOCSSOFTCAR	0x541A
#define TTY_FIONREAD		0x541B
#define TTY_TIOCINQ		FIONREAD
#define TTY_TIOCLINUX		0x541C
#define TTY_TIOCCONS		0x541D
#define TTY_TIOCGSERIAL		0x541E
#define TTY_TIOCSSERIAL		0x541F
#define TTY_TIOCPKT		0x5420
#define TTY_FIONBIO		0x5421
#define TTY_TIOCNOTTY		0x5422
#define TTY_TIOCSETD		0x5423
#define TTY_TIOCGETD		0x5424
#define TTY_TCSBRKP		0x5425
#define TTY_TIOCTTYGSTRUCT	0x5426
#define TTY_TIOCSBRK		0x5427
#define TTY_TIOCCBRK		0x5428
#define TTY_TIOCGSID		0x5429
#define TTY_TIOCGPTN		0x5430
#define TTY_TIOCSPTLCK		0x5431
#define TTY_TCGETX		0x5432
#define TTY_TCSETX		0x5433
#define TTY_TCSETXF		0x5434
#define TTY_TCSETXW		0x5435

/* packet mode */
#define TTY_TIOCPKT_DATA	0x00
#define TTY_TIOCPKT_FLUSHREAD	0x01
#define TTY_TIOCPKT_FLUSHWRITE	0x02
#define TTY_TIOCPKT_STOP	0x04
#define TTY_TIOCPKT_START	0x08
#define TTY_TIOCPKT_NOSTOP	0x10
#define TTY_TIOCPKT_DOSTOP	0x20
#define TTY_TIOCPKT_IOCTL	0x40

/* transmitter empty */
#define TTY_TIOCSER_TEMT	0x01

/* baud rate... :-) */
#define TTY_B0			0000000
#define TTY_B50			0000001
#define TTY_B75			0000002
#define TTY_B110		0000003
#define TTY_B134		0000004
#define TTY_B150		0000005
#define TTY_B200		0000006
#define TTY_B300		0000007
#define TTY_B600		0000010
#define TTY_B1200		0000011
#define TTY_B1800		0000012
#define TTY_B2400		0000013
#define TTY_B4800		0000014
#define TTY_B9600		0000015
#define TTY_B19200		0000016
#define TTY_B38400		0000017

#define TTY_B57600		0010001
#define TTY_B115200		0010002
#define TTY_B230400		0010003
#define TTY_B460800		0010004
#define TTY_B500000		0010005
#define TTY_B576000		0010006
#define TTY_B921600		0010007
#define TTY_B1000000		0010010
#define TTY_B1152000		0010011
#define TTY_B1500000		0010012
#define TTY_B2000000		0010013
#define TTY_B2500000		0010014
#define TTY_B3000000		0010015
#define TTY_B3500000		0010016
#define TTY_B4000000		0010017

/* special characters */
#define TTY_CTRL_AT 		0x00
#define TTY_CTRL_A		0x01
#define TTY_CTRL_B		0x02
#define TTY_CTRL_C		0x03
#define TTY_CTRL_D		0x04
#define TTY_CTRL_E		0x05
#define TTY_CTRL_F		0x06
#define TTY_CTRL_G		0x07
#define TTY_CTRL_H		0x08
#define TTY_CTRL_I		0x09
#define TTY_CTRL_J		0x0a
#define TTY_CTRL_K		0x0b
#define TTY_CTRL_L		0x0c
#define TTY_CTRL_M		0x0d
#define TTY_CTRL_N		0x0e
#define TTY_CTRL_O		0x0f
#define TTY_CTRL_P		0x10
#define TTY_CTRL_Q		0x11
#define TTY_CTRL_R		0x12
#define TTY_CTRL_S		0x13
#define TTY_CTRL_T		0x14
#define TTY_CTRL_U		0x15
#define TTY_CTRL_V		0x16
#define TTY_CTRL_W		0x17
#define TTY_CTRL_X		0x18
#define TTY_CTRL_Y		0x19
#define TTY_CTRL_Z		0x1a
#define TTY_CTRL_LBRACKET	0x1b
#define TTY_CTRL_BSLASH		0x1c
#define TTY_CTRL_RBRACKET	0x1d
#define TTY_CTRL_CTRL		0x1e
#define TTY_CTRL_USCORE		0x1f
#define TTY_CTRL_QMARK		0x7f

/* tty properties */
struct tty_termios {
	uint32_t	c_iflag;
	uint32_t	c_oflag;
	uint32_t	c_cflag;
	uint32_t	c_lflag;
	unsigned char	c_line;
	unsigned char	c_cc[TTY_NCCS];
	uint32_t	__c_ispeed;
	uint32_t	__c_ospeed;
};


/* tty window properties */
struct tty_winsize {
	uint16_t	ws_row;
	uint16_t	ws_col;
	uint16_t	ws_xpixel;
	uint16_t	ws_ypixel;
};


struct tty_winbuffer {
	uint16_t	wb_row;
	uint16_t	wb_col;
	uint16_t	wb_prev_row;
	uint16_t	wb_prev_col;
};


struct tty_winpos {
	uint16_t	wp_x;
	uint16_t	wp_y;
	uint16_t	wp_prev_x;
	uint16_t	wp_prev_y;
};


struct tty_winprops {
	struct tty_winsize	winsize;
	struct tty_winbuffer	winbuffer;
	struct tty_winpos	winpos;
};

#endif
