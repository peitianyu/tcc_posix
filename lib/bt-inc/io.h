/* minimal io.h shim for building TCC -bt/-b runtime objects.
   tcc.h #include <io.h> under _WIN32, but the backtrace-only runtime
   never calls open/close/read/write, so this can be empty. */
#ifndef _BT_IO_SHIM_H_
#define _BT_IO_SHIM_H_
#endif