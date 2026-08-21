/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

/*
 * tcc_posix: /etc 虚拟文件支持 (R9).
 * psxscl 根目录映射到 Windows 系统根 (C:\), 不真实存在 /etc/passwd|group.
 * musl 的 passwd/group 库经 fopen("/etc/passwd") 顺序读取, 故在接口层
 * 挂一个虚拟 /etc 命名空间: config iofn 识别 "passwd"/"group" 文件,
 * 返回只读伪文件内容.
 *
 * 注意 read 只拿到 hfile (不含 ofd), 因此 per-open 读取位置必须由
 * hfile 指向的对象承载. 本实现用 静态槽池 完成 per-open 状态,
 * 避免在系统调用层递归进入 musl malloc. 槽位上限小, 系统调用层无锁,
 * 由调用语义保证 (单文件单流), 若并发超限返回资源不足.
 *
 * 内容与 psx_init 中 __psx.__uid/__gid=1000 保持一致.
 */

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_path.h"
#include "psx_iovtbl.h"
#include "psx.h"

#define PSX_CONFIG_FILES_MAX 8

struct __psx_config_file {
	unsigned char		used;
	const unsigned char *	data;
	uint32_t		length;
	uint32_t		pos;		/* per-open read offset */
};

static struct __psx_config_file __config_files[PSX_CONFIG_FILES_MAX];

static const unsigned char __etc_passwd[] =
	"root:x:1000:1000:root:/root:/bin/sh\n";
static const unsigned char __etc_group[] =
	"root:x:1000:\n";


static struct __psx_config_file * __psx_config_file_claim(
	const unsigned char *	data,
	uint32_t		length)
{
	int32_t i;

	for (i=0; i<PSX_CONFIG_FILES_MAX; i++)
		if (!__config_files[i].used) {
			__config_files[i].used   = 1;
			__config_files[i].data   = data;
			__config_files[i].length = length;
			__config_files[i].pos    = 0;
			return &__config_files[i];
		}

	return 0;
}


static int32_t __fastcall __psx_iofn_config_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t				len;
	wchar16_t *			buf;
	const unsigned char *		data;
	uint32_t			dlen;
	struct __psx_config_file *	file;

	/* the final element (utf16) is in outmark; 长度按字节计 (与 fsroot
	 * 一致, outmark 以 bytes_written 递增), 再换算成 wchar16 字符数 */
	buf = path_info->outmark[0];
	len = (int32_t)(((uintptr_t)path_info->outmark[1]
			- (uintptr_t)path_info->outmark[0])
			/ sizeof(wchar16_t));

	if (len && ((*buf == '/') || (*buf == '\\')))
		buf++, len--;

	data = 0;
	dlen = 0;

	if (len == 6)
		if ((buf[0]=='p') && (buf[1]=='a') && (buf[2]=='s')
		 && (buf[3]=='s') && (buf[4]=='w') && (buf[5]=='d')) {
			data = __etc_passwd;
			dlen = (uint32_t)sizeof(__etc_passwd) - 1;
		}

	if (!data && (len == 5))
		if ((buf[0]=='g') && (buf[1]=='r') && (buf[2]=='o')
		 && (buf[3]=='u') && (buf[4]=='p')) {
			data = __etc_group;
			dlen = (uint32_t)sizeof(__etc_group) - 1;
		}

	if (!data)
		return NT_STATUS_NOT_FOUND;

	if (!(file = __psx_config_file_claim(data, dlen)))
		return NT_STATUS_INSUFFICIENT_RESOURCES;

	/* per-open pseudo-file handle: slot carries offset */
	path_info->hfile = file;

	return NT_STATUS_SUCCESS;
}


static int32_t __stdcall __psx_iofn_config_request_path(
	void **			hparent,
	void *			hdir,
	uintptr_t *		buffer,
	uint32_t		buffer_size,
	uint32_t		desired_access,
	uint32_t		open_options,
	int32_t *		type)
{
	*hparent = (__tlca_self())->ctx->root.hfile;
	*type = PSX_FD_OS_FS_ROOT;
	return NT_STATUS_SUCCESS;
}


static int32_t __stdcall __psx_iofn_config_close(void * handle)
{
	struct __psx_config_file * file;

	file = (struct __psx_config_file *)handle;
	if (file)
		file->used = 0;

	return NT_STATUS_SUCCESS;
}


static int32_t __stdcall __psx_iofn_config_read(
	void *				hfile,
	void *				hevent,
	nt_io_apc_routine *		apc_routine,
	void *				apc_context,
	nt_io_status_block *		iosb,
	void *				buffer,
	uint32_t			bytes_to_read,
	nt_large_integer *		byte_offset,
	uint32_t *			key)
{
	struct __psx_config_file *	file;
	uint32_t			avail, n;

	file = (struct __psx_config_file *)hfile;
	iosb->pointer = 0;

	if (file->pos >= file->length) {
		iosb->status = NT_STATUS_END_OF_FILE;
		iosb->info   = 0;
		return NT_STATUS_END_OF_FILE;
	}

	avail = file->length - file->pos;
	n = (bytes_to_read < avail) ? bytes_to_read : avail;

	__ntapi->tt_generic_memcpy(buffer, file->data + file->pos, n);
	file->pos += n;

	iosb->status = NT_STATUS_SUCCESS;
	iosb->info   = n;

	return NT_STATUS_SUCCESS;
}


void __psx_iofn_config_init(struct __iovtbl * iovtbl)
{
	iovtbl->open_next	= __psx_iofn_config_open_next;

	iovtbl->prolog		= __psx_iofn_default_prolog;
	iovtbl->epilog		= __psx_iofn_default_epilog;

	iovtbl->alloc		= __psx_iofn_default_alloc;
	iovtbl->free		= __psx_iofn_default_free;

	iovtbl->stat		= __psx_iofn_default_stat;
	iovtbl->unlink		= __psx_iofn_default_unlink;

	iovtbl->poll		= __psx_iofn_default_poll;
	iovtbl->peek		= __psx_iofn_default_peek;

	iovtbl->fsync		= __psx_iofn_default_fsync;
	iovtbl->notify		= __psx_iofn_default_notify;

	iovtbl->create		= __psx_iofn_default_create;
	iovtbl->open		= __psx_iofn_default_open;
	iovtbl->close		= __psx_iofn_config_close;

	iovtbl->read		= __psx_iofn_config_read;
	iovtbl->write		= __psx_iofn_default_write;

	iovtbl->fcntl		= __psx_iofn_default_fcntl;
	iovtbl->ioctl		= __psx_iofn_default_ioctl;

	iovtbl->lock		= __psx_iofn_default_lock;
	iovtbl->unlock		= __psx_iofn_default_unlock;

	iovtbl->query		= __psx_iofn_default_query;
	iovtbl->set		= __psx_iofn_default_set;

	iovtbl->cancel		= __psx_iofn_default_cancel;
	iovtbl->remove		= __psx_iofn_default_remove;

	iovtbl->getdents		= __psx_iofn_default_getdents;
	iovtbl->getvents		= __psx_iofn_default_getvents;

	iovtbl->open_logical_parent	= __psx_iofn_config_request_path;
	iovtbl->open_physical_parent	= __psx_iofn_config_request_path;
}