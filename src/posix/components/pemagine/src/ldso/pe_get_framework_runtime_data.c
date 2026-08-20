/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "pe_os.h"

struct pe_framework_cmdline {
	struct pe_guid_str_utf16	guid;
	wchar16_t			space1;
	wchar16_t			rarg[2];
	wchar16_t			space2;
	wchar16_t			addr[2*__SIZEOF_POINTER__];
	wchar16_t			null;
};

static int32_t pe_hex_utf16_to_uint32(
	const wchar16_t	hex_key_utf16[8],
	uint32_t *	key)
{
	int		i;
	unsigned char	uch[8];
	unsigned char	ubytes[4];
	uint32_t *	key_ret;

	/* input validation */
	for (i=0; i<8; i++)
		if (!((hex_key_utf16[i] >= 'a') && (hex_key_utf16[i] <= 'f')))
			if (!((hex_key_utf16[i] >= 'A') && (hex_key_utf16[i] <= 'F')))
				if (!((hex_key_utf16[i] >= '0') && (hex_key_utf16[i] <= '9')))
					return OS_STATUS_INVALID_PARAMETER;

	/* intermediate step: little endian byte order */
	uch[0] = (unsigned char)hex_key_utf16[6];
	uch[1] = (unsigned char)hex_key_utf16[7];
	uch[2] = (unsigned char)hex_key_utf16[4];
	uch[3] = (unsigned char)hex_key_utf16[5];
	uch[4] = (unsigned char)hex_key_utf16[2];
	uch[5] = (unsigned char)hex_key_utf16[3];
	uch[6] = (unsigned char)hex_key_utf16[0];
	uch[7] = (unsigned char)hex_key_utf16[1];

	for (i=0; i<8; i++) {
		/* 'a' > 'A' > '0' */
		if (uch[i] >= 'a')
			uch[i] -= ('a' - 0x0a);
		else if (uch[i] >= 'A')
			uch[i] -= ('A' - 0x0a);
		else
			uch[i] -= '0';
	}

	ubytes[0] = uch[0] * 0x10 + uch[1];
	ubytes[1] = uch[2] * 0x10 + uch[3];
	ubytes[2] = uch[4] * 0x10 + uch[5];
	ubytes[3] = uch[6] * 0x10 + uch[7];

	key_ret = (uint32_t *)ubytes;
	*key = *key_ret;

	return OS_STATUS_SUCCESS;
}

static int32_t pe_hex_utf16_to_uint16(
	const wchar16_t hex_key_utf16[4],
	uint16_t *	key)
{
	int32_t		status;
	uint32_t	dword_key;
	wchar16_t	hex_buf[8] = {'0','0','0','0'};

	hex_buf[4] = hex_key_utf16[0];
	hex_buf[5] = hex_key_utf16[1];
	hex_buf[6] = hex_key_utf16[2];
	hex_buf[7] = hex_key_utf16[3];

	if ((status = pe_hex_utf16_to_uint32(hex_buf,&dword_key)))
		return status;

	*key = (uint16_t)dword_key;

	return OS_STATUS_SUCCESS;
}

static int32_t pe_hex_utf16_to_uint64(
	const wchar16_t	hex_key_utf16[16],
	uint64_t *	key)
{
	int32_t		status;
	uint32_t	x64_key[2];
	uint64_t *	key_ret;

	if ((status = pe_hex_utf16_to_uint32(
			&hex_key_utf16[0],
			&x64_key[1])))
		return status;

	if ((status = pe_hex_utf16_to_uint32(
			&hex_key_utf16[8],
			&x64_key[0])))
		return status;

	key_ret = (uint64_t *)x64_key;
	*key    = *key_ret;

	return OS_STATUS_SUCCESS;
}


static int32_t pe_hex_utf16_to_uintptr(
	const wchar16_t	hex_key_utf16[],
	uintptr_t *	key)
{
	(void)pe_hex_utf16_to_uint32;
	(void)pe_hex_utf16_to_uint64;

	#if (__SIZEOF_POINTER__ == 4)
		return pe_hex_utf16_to_uint32(hex_key_utf16,key);
	#elif (__SIZEOF_POINTER__ == 8)
		return pe_hex_utf16_to_uint64(hex_key_utf16,key);
	#endif
}

static int32_t pe_string_to_guid_utf16(
	const struct pe_guid_str_utf16 *guid_str,
	struct pe_guid *		guid)
{
	int32_t			status;
	uint16_t		key;
	const wchar16_t *	wch;

	if ((guid_str->lbrace != '{')
			|| (guid_str->rbrace != '}')
			|| (guid_str->dash1 != '-')
			|| (guid_str->dash2 != '-')
			|| (guid_str->dash3 != '-')
			|| (guid_str->dash4 != '-'))
		return OS_STATUS_INVALID_PARAMETER;

	wch = &(guid_str->group5[0]);

	if ((status = pe_hex_utf16_to_uint32(
			guid_str->group1,
			&guid->data1)))
		return status;

	if ((status = pe_hex_utf16_to_uint16(
			guid_str->group2,
			&guid->data2)))
		return status;

	if ((status = pe_hex_utf16_to_uint16(
			guid_str->group3,
			&guid->data3)))
		return status;

	if ((status = pe_hex_utf16_to_uint16(
			guid_str->group4,
			&key)))
		return status;

	guid->data4[0] = key >> 8;
	guid->data4[1] = key % 0x100;

	if ((status = pe_hex_utf16_to_uint16(
			&(wch[0]),
			&key)))
		return status;

	guid->data4[2] = key >> 8;
	guid->data4[3] = key % 0x100;

	if ((status = pe_hex_utf16_to_uint16(
			&(wch[4]),
			&key)))
		return status;

	guid->data4[4] = key >> 8;
	guid->data4[5] = key % 0x100;

	if ((status = pe_hex_utf16_to_uint16(
			&(wch[8]),
			&key)))
		return status;

	guid->data4[6] = key >> 8;
	guid->data4[7] = key % 0x100;

	return OS_STATUS_SUCCESS;
}

static int32_t pe_guid_compare(
	const struct pe_guid *	pguid_dst,
	const struct pe_guid *	pguid_src)
{
	const uint32_t *	dst;
	const uint32_t *	src;

	dst = &pguid_dst->data1;
	src = &pguid_src->data1;

	if ((dst[0] == src[0])
			&& (dst[1] == src[1])
			&& (dst[2] == src[2])
			&& (dst[3] == src[3]))
		return OS_STATUS_SUCCESS;

	return OS_STATUS_NO_MATCH;
}

int32_t pe_get_framework_runtime_data(
	struct pe_framework_runtime_data **	rtdata,
	const wchar16_t *			cmdline,
	const struct pe_guid *			abi)
{
	int32_t					status;
	struct pe_framework_runtime_data *	prtdata;
	struct pe_framework_cmdline *		fcmdline;
	struct pe_guid				guid;
	uintptr_t				address;
	uintptr_t				buffer;
	void *					hntdll;
	os_zw_read_virtual_memory *		zw_read_virtual_memory;

	/* init */
	if (!(hntdll = pe_get_ntdll_module_handle()))
		return OS_STATUS_INTERNAL_ERROR;

	if (!(zw_read_virtual_memory = (os_zw_read_virtual_memory *)pe_get_procedure_address(
			hntdll,"ZwReadVirtualMemory")))
		return OS_STATUS_INTERNAL_ERROR;

	/* framework cmdline */
	if (!(fcmdline = (struct pe_framework_cmdline *)cmdline))
		return OS_STATUS_INVALID_PARAMETER;

	/* framework cmdline: conformance */
	if (fcmdline->null)
		return OS_STATUS_INVALID_PARAMETER;

	/* framework cmdline: rarg */
	if ((fcmdline->space1 != ' ')
			|| (fcmdline->space2  != ' ')
			|| (fcmdline->rarg[0] != '-')
			|| (fcmdline->rarg[1] != 'r'))
		return OS_STATUS_INVALID_PARAMETER;

	/* framework cmdline: address */
	if ((status = pe_hex_utf16_to_uintptr(
			fcmdline->addr,
			&address)))
		return status;

	/* framework cmdline: guid */
	if ((status = pe_string_to_guid_utf16(
			&fcmdline->guid,
			&guid)))
		return status;

	/* framework cmdline: abi */
	if ((status = pe_guid_compare(&guid,abi)))
		return status;

	/* address: alignment */
	if (address & 0xFFF)
		return OS_STATUS_INVALID_ADDRESS;

	/* address is aligned at page boundary */
	if ((status = zw_read_virtual_memory(
			OS_CURRENT_PROCESS_HANDLE,
			(void *)address,
			(char *)&buffer,
			sizeof(buffer),
			0)))
		return status;

	/* rtdata */
	prtdata = (struct pe_framework_runtime_data *)address;

	/* rtdata: abi */
	if (pe_guid_compare(&prtdata->abi,abi))
		return OS_STATUS_CONTEXT_MISMATCH;

	/* yay */
	*rtdata = prtdata;

	return OS_STATUS_SUCCESS;
}
