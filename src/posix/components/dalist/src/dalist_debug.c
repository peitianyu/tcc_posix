/*****************************************************************************/
/*  dalist: a zero-dependency book-keeping library                           */
/*  Copyright (C) 2013--2021  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.DALIST.                      */
/*****************************************************************************/

#include <dalist/dalist.h>
#include "dalist_impl.h"

dalist_api
int dalist_debug_setup(
	struct dalist_ex *	dlist,
	struct dalist_debug *	dlist_debug,
	enum dalist_dbgenv	dbgenv,
	dalist_sprintf *	pfn_sprintf,
	dalist_write *		pfn_write,
	dalist_write_file *	pfn_write_file)
{
	if (dlist)
		/* in case we fail */
		dlist->debug = 0;
	else
		return DALIST_ELIST;

	if (!dlist_debug)
		return DALIST_EDEBUGSTRUCT;
	else if (!pfn_sprintf)
		return DALIST_EDEBUGSPRINTF;

	switch (dbgenv) {
		case DALIST_DEBUG_ENV_POSIX:
			if (!pfn_write)
				return DALIST_EDEBUGWRITE;
			break;

		case DALIST_DEBUG_ENV_NT:
			if (!pfn_write_file)
				return DALIST_EDEBUGWRITEFILE;
			break;

		default:
			return DALIST_EDEBUGENV;
	}

	dlist_debug->dbgenv        = dbgenv;
	dlist_debug->sprintf       = pfn_sprintf;
	dlist_debug->write         = pfn_write;
	dlist_debug->zw_write_file = pfn_write_file;

	dlist->debug = dlist_debug;

	return DALIST_OK;
}


static const char dalist_fmt_header_32[] =
	"dlist:\t\t0x%08x\n"
	"dlist->head:\t0x%08x\n"
	"dlist->tail:\t0x%08x\n\n";

static const char dalist_fmt_header_64[] =
	"dlist:\t\t0x%016x\n"
	"dlist->head:\t0x%016x\n"
	"dlist->tail:\t0x%016x\n\n";

static const char dalist_fmt_node_32[] =
	"node: 0x%08x\t"
	"prev: 0x%08x\t"
	"next: 0x%08x\t"
	"any:  0x%08x\t\n";

static const char dalist_fmt_node_64[] =
	"node: 0x%016x\t"
	"prev: 0x%016x\t"
	"next: 0x%016x\t"
	"any:  0x%016x\t\n";

static const char * dalist_fmt_headers[2] = {
	dalist_fmt_header_32,
	dalist_fmt_header_64
};

static const char * dalist_fmt_nodes[2] = {
	dalist_fmt_node_32,
	dalist_fmt_node_64
};


static int dalist_dbg_write_posix(
	struct dalist_ex *	dlist,
	intptr_t		fildes_or_hfile,
	const void *		buf,
	size_t			nbyte)
{
	int		fildes;
	dalist_write *	pfn_write;
	ssize_t		bytes_written;

	fildes = (int)fildes_or_hfile;
	pfn_write = (dalist_write *)dlist->debug->write;

	bytes_written = pfn_write(fildes,buf,nbyte);

	if (bytes_written < 0)
		return DALIST_EDEBUGENV;

	else if (bytes_written == (ssize_t)nbyte)
		return DALIST_OK;

	else
		return DALIST_EDEBUGENV;
}


static int dalist_dbg_write_nt(
	struct dalist_ex *	dlist,
	intptr_t		fildes_or_hfile,
	const void *		buf,
	size_t			nbyte)
{
	void *			hfile;
	struct dalist_iosb	iosb;
	dalist_write_file *	pfn_write_file;

	hfile          = (void *)fildes_or_hfile;
	pfn_write_file = (dalist_write_file *)dlist->debug->zw_write_file;

	return pfn_write_file(
		hfile,
		(void *)0,
		(void *)0,
		(void *)0,
		&iosb,
		(void *)buf,
		(uint32_t)nbyte,
		(int64_t *)0,
		(uint32_t *)0);
}


dalist_api
int dalist_debug_print(
	struct dalist_ex *	dlist,
	intptr_t		fildes_or_hfile)
{
	const char *		fmt_header;
	const char *		fmt_node;
	char			dbg_buf[128];
	intptr_t		nbyte;

	int			status;
	struct dalist_node *	node;
	dalist_sprintf *	dbg_sprintf;
	dalist_dbg_write *	dbg_write;

	if (!dlist)
		return DALIST_ELIST;
	else if (!dlist->debug)
		return DALIST_EDEBUGENV;

	/* initial setup */
	dbg_sprintf = (dalist_sprintf *)dlist->debug->sprintf;
	fmt_header  = dalist_fmt_headers[(sizeof(size_t)-4)/4];
	fmt_node    = dalist_fmt_nodes[(sizeof(size_t)-4)/4];

	if (dlist->debug->dbgenv == DALIST_DEBUG_ENV_POSIX)
		dbg_write = dalist_dbg_write_posix;
	else if (dlist->debug->dbgenv == DALIST_DEBUG_ENV_NT)
		dbg_write = dalist_dbg_write_nt;
	else
		dbg_write = 0;

	if ((!dbg_sprintf) || (!dbg_write))
		return DALIST_EDEBUGENV;

	/* print list header */
	nbyte = dbg_sprintf((char *)dbg_buf,fmt_header,dlist,dlist->head,dlist->tail);

	if (nbyte < 0)
		return DALIST_EDEBUGENV;

	status = dbg_write(dlist,fildes_or_hfile,dbg_buf,nbyte);

	if (status != NT_STATUS_SUCCESS)
		return DALIST_EDEBUGENV;

	/* print node information */
	status = DALIST_OK;
	node   = (struct dalist_node *)dlist->head;

	while (node && (status == DALIST_OK)) {
		/* create output line */
		nbyte = dbg_sprintf(
			(char *)dbg_buf,
			fmt_node,
			node,
			node->prev,
			node->next,
			node->any);

		if (nbyte < 0)
			return DALIST_EDEBUGENV;

		/* write output line */
		status = dbg_write(
			dlist,
			fildes_or_hfile,
			dbg_buf,
			nbyte);

		node = node->next;
	}

	return DALIST_OK;
}
