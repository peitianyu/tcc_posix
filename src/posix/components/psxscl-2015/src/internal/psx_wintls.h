#ifndef _PSX_WINTLS_H_
#define _PSX_WINTLS_H_

#include <psxtypes/psxtypes.h>

#define WINAPI_TLS_OUT_OF_INDEXES (uint32_t)(-1)

typedef uint32_t __stdcall winapi_tls_alloc(void);
typedef int32_t  __stdcall winapi_tls_free(uint32_t index);
typedef void *   __stdcall winapi_tls_get_value(uint32_t index);
typedef int32_t  __stdcall winapi_tls_set_value(uint32_t index, void * tls_value);

#endif
