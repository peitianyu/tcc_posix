#ifndef _PSX_PTRACE_H
#define _PSX_PTRACE_H

#if defined(__X86_MODEL)
#include "bits/i386/psx_ptrace.h"
#elif defined(__X86_64_MODEL)
#include "bits/x86_64/psx_ptrace.h"
#endif

#endif
