/* ------------------------------------------------------------- */
/*  lib/bt-inc/windows.h - minimal winapi shim for TCC -bt/-b
 *  runtime objects (bt-exe.o, bcheck.o).
 *
 *  tcc_posix is a musl-only toolchain: it ships no winapi headers.
 *  The only Windows API the crash-backtrace runtime needs on x86_64
 *  is a vectored exception handler + a virtual-memory query, both of
 *  which are bound at runtime via psx_btwin.c (dynamic ntdll/kernel32
 *  resolution).  This header supplies exactly those declarations so
 *  tccrun.c (which includes tcc.h -> <windows.h>) compiles against
 *  musl headers without pulling in a full winapi tree.
 *
 *  This directory is NOT installed into bin/include; it is used only
 *  when building the bt runtime objects (see script/build_bt.sh).
 * ------------------------------------------------------------- */
#ifndef _BT_WINDOWS_SHIM_H_
#define _BT_WINDOWS_SHIM_H_

/* On x86_64 the Windows calling convention is unified, so __stdcall is a
   no-op.  tccrun.c writes `static long __stdcall cpu_exception_handler(...)`
   under _WIN32, and this tcc build does not parse __stdcall as a keyword
   (it is only provided by the winapi headers), so neutralize it here. */
#ifndef __stdcall
# define __stdcall
#endif
#define WINAPI __stdcall

typedef unsigned long          DWORD;
        typedef unsigned short         WORD;
        typedef int                    BOOL;
typedef void                  *PVOID;
typedef void                  *HANDLE;
typedef void                  *HMODULE;
typedef unsigned long long     ULONG_PTR;

#ifndef __inline
# define __inline inline
#endif

/* tcc's src/elf.h typedefs int8_t..int64_t under _WIN32 guarded by
   __int8_t_defined.  We build against musl's <stdint.h> which already
   provides these (as `long`, not `long long`), so claim the guard and
   let elf.h reuse musl's exact-width integer types instead of
   re-typedef'ing them with an incompatible type. */
#ifndef __int8_t_defined
# define __int8_t_defined
#endif

/* x64 CONTEXT - MUST mirror the real Windows CONTEXT (winnt.h) layout,
           because this struct is used to interpret a CONTEXT* handed to the
           VEH by the OS.  Only the frame-walk fields (Rip/Rbp/Rsp) are read by
           rt_getcontext(), but the offsets must match the real structure:
           Rax=0x78 Rcx=0x80 Rdx=0x88 Rbx=0x90 Rsp=0x98 Rbp=0xa0 Rsi=0xa8
           Rdi=0xb0 R8=0xb8 .. R15=0xf0 Rip=0xf8                      */
        typedef struct _CONTEXT {
            ULONG_PTR P1Home, P2Home, P3Home, P4Home, P5Home, P6Home; /* 0x00 */
            DWORD   ContextFlags;                                     /* 0x30 */
            DWORD   MxCsr;                                            /* 0x34 */
            WORD    SegCs, SegDs, SegEs, SegFs, SegGs, SegSs;         /* 0x38 */
            DWORD   EFlags;                                           /* 0x44 */
            ULONG_PTR Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;                   /* 0x48 */
            ULONG_PTR Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi;         /* 0x78 */
            ULONG_PTR R8, R9, R10, R11, R12, R13, R14, R15;           /* 0xb8 */
            ULONG_PTR Rip;                                            /* 0xf8 */
        } CONTEXT;

typedef struct _EXCEPTION_RECORD {
    DWORD   ExceptionCode;
    DWORD   ExceptionFlags;
    struct _EXCEPTION_RECORD *ExceptionRecord;
    PVOID   ExceptionAddress;
    DWORD   NumberParameters;
    PVOID   ExceptionInformation[15];
} EXCEPTION_RECORD;

typedef struct _EXCEPTION_POINTERS {
    EXCEPTION_RECORD *ExceptionRecord;
    CONTEXT          *ContextRecord;
} EXCEPTION_POINTERS;

#define EXCEPTION_CONTINUE_SEARCH        (0)
#define EXCEPTION_EXECUTE_HANDLER        (1)
#define EXCEPTION_ACCESS_VIOLATION       ((DWORD)0xC0000005)
#define EXCEPTION_INT_DIVIDE_BY_ZERO     ((DWORD)0xC0000094)
#define EXCEPTION_STACK_OVERFLOW         ((DWORD)0xC00000FD)
#define EXCEPTION_BREAKPOINT             ((DWORD)0x80000003)
#define EXCEPTION_SINGLE_STEP            ((DWORD)0x80000004)

typedef long (*PVECTORED_EXCEPTION_HANDLER)(EXCEPTION_POINTERS *);
void *AddVectoredExceptionHandler(unsigned long First,
                                  PVECTORED_EXCEPTION_HANDLER Handler);

typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID          BaseAddress;
    PVOID          AllocationBase;
    DWORD          AllocationProtect;
    unsigned long long RegionSize;
    DWORD          State;
    DWORD          Protect;
    DWORD          Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

unsigned long long VirtualQuery(const void *lpAddress,
                                PMEMORY_BASIC_INFORMATION lpBuffer,
                                unsigned long long dwLength);

#endif /* _BT_WINDOWS_SHIM_H_ */