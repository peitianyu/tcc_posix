#if defined(__X86_MODEL)
#if 	(__COMPILER__ == __GCC__)
#include "i386/nt_atomic_i386_asm__gcc.h"
#elif	(__COMPILER__ == __MSVC__)
#include "i386/nt_atomic_i386_asm__msvc.h"
#endif

#elif defined(__X86_64_MODEL)
#if 	(__COMPILER__ == __GCC__)
#include "x86_64/nt_atomic_x86_64_asm__gcc.h"
#elif	(__COMPILER__ == __MSVC__)
#include "x86_64/nt_atomic_x86_64_asm__msvc.h"
#endif

#endif
