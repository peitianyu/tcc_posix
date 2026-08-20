#include <psxtypes/psxtypes.h>
#include <psxtypes/section/freestd.h>

__attr_section_decl__(".freestd")
static const void * const pe_affiliation
	__attr_section__(".freestd")
	= 0;

#ifdef  PE_LDSO
#define pe_entry_point __ldso_entry_point
#endif

int __stdcall __attr_protected__ pe_entry_point(
	void *   hinstance,
	uint32_t reason,
	void *   reserved)
{
	(void)pe_affiliation;
	(void)hinstance;
	(void)reason;
	(void)reserved;

	return 1;
}
