/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_flags.h"
#include "psx_mman.h"
#include "psx_signal.h"
#include "psx_ofd.h"
#include "psx_acl.h"
#include "psx.h"

static const struct __flag_set __mmap_section_access[] = {
	{PROT_READ,	NT_SECTION_MAP_READ},
	{PROT_WRITE,	NT_SECTION_MAP_WRITE},
	{PROT_EXEC,	NT_SECTION_MAP_EXECUTE},
	{PROT_NONE,	0},
	{0,		0}};

static const struct __flag_set __mmap_section_prot[] = {
	{PROT_READ,	NT_PAGE_READONLY},
	{PROT_WRITE,	NT_PAGE_READWRITE},
	{PROT_EXEC,	NT_PAGE_EXECUTE},
	{PROT_NONE,	0},
	{0,		0}};

static const struct __flag_set __mmap_section_share[] = {
	{MAP_SHARED,	0},
	{MAP_PRIVATE,	NT_PAGE_WRITECOPY},
	{0,		0}};

static const struct __flag_set __mmap_flags[] = {
	{MAP_SHARED,	0},
	{MAP_PRIVATE,	0},
	{MAP_32BIT,	0},
	{MAP_ANON,	0},
	{MAP_DENYWRITE,	0},
	{MAP_EXECUTABLE,0},
	{MAP_FILE,	0},
	{MAP_FIXED,	0},
	{MAP_GROWSDOWN,	0},
	{MAP_HUGETLB,	0},
	{MAP_LOCKED,	0},
	{MAP_NONBLOCK,	0},
	{MAP_NORESERVE,	0},
	{MAP_POPULATE,	0},
	{MAP_STACK,	0},
	{0,		0}};


__psx_api
void * __sys_mmap(
	void *	addr,
	size_t	length,
	int	prot,
	int	flags,
	int	fd,
	off_t	offset)
{
	int32_t			status;
	struct __mmap_ctx	m;
	struct __psx_ctx *	ctx;

	/* prolog */
	__ntapi->tt_aligned_block_memset(
		&m,0,sizeof(m));

	m.tlca = __tlca_self();
	if (!(ctx = __tlca_shared_ctx(m.tlca))) return 0;
	__psx_sig_prolog(m.tlca);


	/* semantics */
	if ((flags & MAP_SHARED) && (flags & MAP_PRIVATE))
		return (void *)__psx_sig_epilog(m.tlca,-EINVAL,EPSXONLY);
	else if ((flags & (MAP_SHARED | MAP_PRIVATE)) == 0)
		return (void *)__psx_sig_epilog(m.tlca,-EINVAL,EPSXONLY);

	if (flags & MAP_ANON) {
		fd	= 0;
		offset	= 0;
	}

	addr = (flags & MAP_FIXED) ? addr : 0;

	/* section */
	if (__psx_convert_flags_to_native(
			__mmap_section_access,
			prot,&m.fsection,0))
		return (void *)__psx_sig_epilog(m.tlca,-EINVAL,EPSXONLY);

	/* protection */
	if (__psx_convert_flags_to_native(
			__mmap_section_prot,
			prot,&m.cprot,0))
		return (void *)__psx_sig_epilog(m.tlca,-EINVAL,EPSXONLY);

	__psx_convert_flags_to_native(
			__mmap_section_share,
			flags,&m.mprot,0);

	/* flags */
	if (__psx_convert_flags_to_native(
			__mmap_flags,
			flags,&m.fpage,0))
		return (void *)__psx_sig_epilog(m.tlca,-EINVAL,EPSXONLY);

	/* ofd */
	if (fd) {
		if (!(m.ofd = __psx_ofd_ref_inc(ctx,fd)))
			return (void *)__psx_sig_epilog(m.tlca,-EBADF,EPSXONLY);

		m.hfile = m.ofd->info.hfile;
		__psx_ofd_ref_dec(ctx,m.ofd);
	}

	/* internal section */
	m.oa.len	= sizeof(m.oa);
	m.oa.root_dir	= 0;
	m.oa.obj_name	= 0;
	m.oa.obj_attr	= NT_OBJ_INHERIT;
	m.oa.sec_desc	= __PSX_DEF_SEC_DESC;
	m.oa.sec_qos	= __PSX_DEF_SEC_QOS;

	m.addr		= addr;
	m.attr		= m.hfile ? NT_SEC_COMMIT : NT_SEC_RESERVE;
	m.size.quad	= (length < 1<<30) ? 1<<30 : length;
	m.foffset.quad	= offset;
	m.commit	= length;
	m.reserve	= (size_t)m.size.quad;
	m.share		= (m.hfile && (flags & MAP_PRIVATE)) ? NT_VIEW_UNMAP : NT_VIEW_SHARE;
	m.cprot		= (m.cprot & NT_PAGE_READWRITE) ? NT_PAGE_READWRITE : m.cprot;
	if (!m.cprot)
		m.cprot	= NT_PAGE_NOACCESS;
	m.mprot		= (m.mprot && m.hfile) ? m.mprot : m.cprot;

	if (m.hfile) {
		if ((status = __ntapi->zw_create_section(
				&m.hsection,
				m.fsection | NT_SECTION_QUERY | NT_SECTION_EXTEND_SIZE,
				&m.oa,&m.size,m.cprot,m.attr,m.hfile)))
			return (void *)__psx_sig_epilog(m.tlca,-ENOMEM,status);

		if ((status = __ntapi->zw_map_view_of_section(
				m.hsection,
				NT_CURRENT_PROCESS_HANDLE,
				&m.addr,0,m.reserve,&m.foffset,&m.reserve,
				m.share,0,m.mprot)))
			return (void *)__psx_sig_epilog(m.tlca,-ENOMEM,status);
	} else {
		if ((status = __ntapi->zw_allocate_virtual_memory(
				NT_CURRENT_PROCESS_HANDLE,
				&m.addr,0,
				&m.reserve,
				NT_MEM_RESERVE,
				m.cprot)))
			return (void *)__psx_sig_epilog(m.tlca,-ENOMEM,status);

		if ((status = __ntapi->zw_allocate_virtual_memory(
				NT_CURRENT_PROCESS_HANDLE,
				&m.addr,0,
				&m.commit,
				NT_MEM_COMMIT,
				m.mprot)))
			return (void *)__psx_sig_epilog(m.tlca,-ENOMEM,status);
	}

	/* mman */
	if ((status = __psx_section_add(ctx,&m)))
		return (void *)__psx_sig_epilog(m.tlca,-ENOMEM,status);

	return (void *)__psx_sig_epilog(m.tlca,(intptr_t)m.addr,status);
}
