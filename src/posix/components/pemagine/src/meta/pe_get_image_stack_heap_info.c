/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pe_consts.h>
#include <pemagine/pe_structs.h>
#include <pemagine/pemagine.h>

static uint32_t pe_read_raw_data_32(unsigned char * addr)
{
	uint32_t * p32;

	p32 = (uint32_t *)addr;
	return *p32;
}

static uint64_t pe_read_raw_data_64(unsigned char * addr)
{
	uint64_t * p64;

	p64 = (uint64_t *)addr;
	return *p64;
}

int pe_get_image_stack_heap_info(const void * base, struct pe_stack_heap_info * stack_heap_info)
{
	uint16_t *		magic;
	union pe_raw_opt_hdr *	hdr;

	if (!(hdr = pe_get_image_opt_hdr_addr(base)))
		return 0;

	magic = (uint16_t *)hdr;

	switch (*magic) {
		case PE_MAGIC_PE32:
			stack_heap_info->size_of_stack_reserve = pe_read_raw_data_32(hdr->opt_hdr_32.coh_size_of_stack_reserve);
			stack_heap_info->size_of_stack_commit  = pe_read_raw_data_32(hdr->opt_hdr_32.coh_size_of_stack_commit);
			stack_heap_info->size_of_heap_reserve  = pe_read_raw_data_32(hdr->opt_hdr_32.coh_size_of_heap_reserve);
			stack_heap_info->size_of_heap_commit   = pe_read_raw_data_32(hdr->opt_hdr_32.coh_size_of_heap_commit);
			break;

		case PE_MAGIC_PE32_PLUS:
			stack_heap_info->size_of_stack_reserve = pe_read_raw_data_64(hdr->opt_hdr_64.coh_size_of_stack_reserve);
			stack_heap_info->size_of_stack_commit  = pe_read_raw_data_64(hdr->opt_hdr_64.coh_size_of_stack_commit);
			stack_heap_info->size_of_heap_reserve  = pe_read_raw_data_64(hdr->opt_hdr_64.coh_size_of_heap_reserve);
			stack_heap_info->size_of_heap_commit   = pe_read_raw_data_64(hdr->opt_hdr_64.coh_size_of_heap_commit);
			break;

		default:
			return -1;
	}

	return 0;
}
