/* minimal direct.h shim for building TCC -bt/-b runtime objects.
   tcc.h #include <direct.h> under _WIN32; the backtrace-only runtime
   never calls getcwd(), so this can be empty. */
#ifndef _BT_DIRECT_SHIM_H_
#define _BT_DIRECT_SHIM_H_
#endif