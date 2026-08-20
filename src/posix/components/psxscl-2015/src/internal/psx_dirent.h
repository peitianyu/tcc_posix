#ifndef _PSX_DIRENT_H_
#define _PSX_DIRENT_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"

#define DT_UNKNOWN       0
#define DT_FIFO		 1
#define DT_CHR		 2
#define DT_DIR		 4
#define DT_BLK		 6
#define DT_REG		 8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14

struct __dirent {
	ino_t		d_ino;
	off_t		d_off;
	uint16_t	d_reclen;
	unsigned char	d_type;
	char		d_name[256];
};

struct __dirctx {
	struct __ofd *	dir;
	size_t		reserve;
	size_t		commit;
	uint32_t	used;
	uint32_t	free;
	nt_fsdirent *	next;
	nt_fsdirent	fsdirents;
};

int32_t __psx_dirent_query(struct __psx_tlca * tlca, struct __ofd *, struct __dirent *, unsigned int, nt_iosb *);
int32_t __psx_dirent_seek(struct __ofd *, off_t * offset, int whence);
int32_t __psx_dirent_free(struct __ofd *);

#endif
