#ifndef DALIST_API_H
#define DALIST_API_H

/* host type (posix-libc/free-standing) */
#include "dalist_env.h"

/* dalist_export */
#if	defined(__attr_export__)
#define dalist_export __attr_export__
#else
#define dalist_export
#endif

/* dalist_import */
#if	defined(__attr_import__)
#define dalist_import __attr_import__
#else
#define dalist_import
#endif

/* protected visibility */
#if	defined(__attr_protected__)
#define dalist_protected __attr_protected__
#else
#define dalist_protected
#endif

/* dalist_api */
#if     defined (DALIST_EXPORT)
#define dalist_api dalist_export
#elif   defined (DALIST_IMPORT)
#define dalist_api dalist_import
#elif   defined (DALIST_STATIC)
#define dalist_api dalist_protected
#else
#define dalist_api
#endif

#endif
