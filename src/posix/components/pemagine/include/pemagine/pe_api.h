#ifndef PE_API_H
#define PE_API_H

/* portable integer types */
#if defined (_MIDIPIX_FREESTANDING)
#include <psxtypes/psxtypes.h>
#else
#include <stdint.h>
#include <stddef.h>
typedef  unsigned short wchar16_t;
#endif

/* pe_export */
#if	defined(__attr_export__)
#define pe_export __attr_export__
#else
#define pe_export
#endif

/* pe_import */
#if	defined(__attr_import__)
#define pe_import __attr_import__
#else
#define pe_import
#endif

/* protected visibility */
#if	defined(__attr_protected__)
#define pe_protected __attr_protected__
#else
#define pe_protected
#endif

/* pe_api */
#if     defined (PE_LDSO)
#define pe_api pe_protected
#elif   defined (PE_EXPORT)
#define pe_api pe_export
#elif   defined (PE_IMPORT)
#define pe_api pe_import
#elif   defined (PE_STATIC)
#define pe_api pe_protected
#else
#define pe_api
#endif

#endif /* _PE_API_H_ */
