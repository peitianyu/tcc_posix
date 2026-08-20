#ifndef _NT_ATOM_H_
#define _NT_ATOM_H_

#include <psxtypes/psxtypes.h>

typedef enum _nt_atom_info_class {
	NT_ATOM_BASIC_INFORMATION,
	NT_ATOM_LIST_INFORMATION
} nt_atom_info_class;


typedef uint16_t	nt_atom;


typedef struct _nt_atom_basic_information {
	uint16_t	reference_count;
	uint16_t	pinned;
	uint16_t	name_length;
	wchar16_t	name[];
} nt_atom_basic_information;


typedef struct _nt_atom_list_information {
	uint32_t	number_of_atoms;
	nt_atom		atoms[2];
} nt_atom_list_information;


typedef int32_t __stdcall ntapi_zw_add_atom(
	__in	wchar16_t	str,
	__in	uint16_t	str_length,
	__out	uint16_t *	atom);


typedef int32_t __stdcall ntapi_zw_find_atom(
	__in	wchar16_t	str,
	__in	uint16_t	str_length,
	__out	uint16_t *	atom);


typedef int32_t __stdcall ntapi_zw_delete_atom(
	__in	uint16_t *	atom);


typedef int32_t __stdcall ntapi_zw_query_information_atom(
	__in	uint16_t *		atom,
	__in	nt_atom_info_class	atom_info_class,
	__out	void *			atom_info,
	__in	uint32_t		atom_info_length,
	__out	uint32_t *		returned_length		__optional);

#endif
