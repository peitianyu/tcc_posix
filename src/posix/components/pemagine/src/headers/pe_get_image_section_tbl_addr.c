/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pe_consts.h>
#include <pemagine/pe_structs.h>
#include <pemagine/pemagine.h>
#include "pe_impl.h"


struct pe_raw_sec_hdr * pe_get_image_section_tbl_addr(const void * base)
{
	struct pe_raw_coff_image_hdr *	coff;
	union  pe_raw_opt_hdr *		opt;
	unsigned char *			mark;

	if (!(coff = pe_get_image_coff_hdr_addr(base)))
		return 0;

	if (!(opt = pe_get_image_opt_hdr_addr(base)))
		return 0;

	mark  = opt->opt_hdr_32.coh_magic;
	mark += coff->cfh_size_of_opt_hdr[1] << 8;
	mark += coff->cfh_size_of_opt_hdr[0];

	return (struct pe_raw_sec_hdr *)mark;
}


struct pe_raw_sec_hdr * pe_get_image_named_section_addr(const void * base, const char * name)
{
	uint16_t			count;
	struct pe_raw_sec_hdr *		hdr;
	struct pe_raw_coff_image_hdr *	coff;
	char *				ch;
	size_t				len;
	uint32_t			pos;
	uint64_t			sname;
	uint64_t *			shname;

	if (!(hdr = pe_get_image_section_tbl_addr(base)))
		return 0;

	if (!(coff = pe_get_image_coff_hdr_addr(base)))
		return 0;

	count  = coff->cfh_num_of_sections[1] << 8;
	count += coff->cfh_num_of_sections[0];

	if ((len = pe_impl_strlen_ansi(name)) > 8) {
		/* todo: long name support */
		return 0;

	} else if (len == 0) {
		return 0;

	} else {
		sname = 0;
		ch    = (char *)&sname;

		for (pos=0; pos<len; pos++)
			ch[pos] = name[pos];

		for (; count; hdr++,count--) {
			shname = (uint64_t *)&hdr->sh_name[0];

			if (*shname == sname)
				return hdr;
		}
	}

	return 0;
}


struct pe_raw_sec_hdr * pe_get_image_block_section_addr(
	const void * base,
	uint32_t     blk_rva,
	uint32_t     blk_size)
{
	uint32_t			low,size;
	uint16_t			count;
	struct pe_raw_sec_hdr *		hdr;
	struct pe_raw_coff_image_hdr *	coff;

	if (!(hdr = pe_get_image_section_tbl_addr(base)))
		return 0;

	if (!(coff = pe_get_image_coff_hdr_addr(base)))
		return 0;

	count  = coff->cfh_num_of_sections[1] << 8;
	count += coff->cfh_num_of_sections[0];

	for (; count; hdr++,count--) {
		low   = hdr->sh_virtual_addr[3] << 24;
		low  += hdr->sh_virtual_addr[2] << 16;
		low  += hdr->sh_virtual_addr[1] << 8;
		low  += hdr->sh_virtual_addr[0];

		size  = hdr->sh_virtual_size[3] << 24;
		size += hdr->sh_virtual_size[2] << 16;
		size += hdr->sh_virtual_size[1] << 8;
		size += hdr->sh_virtual_size[0];

		if ((low <= blk_rva) && (blk_rva + blk_size <= low + size))
			return hdr;
	}

	return 0;
}
