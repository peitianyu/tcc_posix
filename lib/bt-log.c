/* ------------------------------------------------------------- */
/* function to get a stack backtrace on demand with a message */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#undef __attribute__

#ifdef CONFIG_TCC_MUSL_STDIO
/* tcc_posix: route stdio to musl (see src/tccrun.c) — undo msvcrt remap.
   stdio.h already declares FILE *const stdin/stdout/stderr; do not redeclare
   them with different const-ness (would be a redefinition error).  musl's own
   #define stderr->(stderr) self-reference resolves to that extern, so the
   identifiers are usable directly without our own externs. */
#undef stdin
#undef stdout
#undef stderr
#undef snprintf
#undef vsnprintf
int snprintf(char *s, size_t n, const char *format, ...);
int vsnprintf(char *s, size_t n, const char *format, va_list arg);
#endif

#ifdef _WIN32
# define DLL_EXPORT __declspec(dllexport)
#else
# define DLL_EXPORT
#endif

/* Needed when using ...libtcc1-usegcc=yes in lib/Makefile */
#if (defined(__GNUC__) && (__GNUC__ >= 6)) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address"
#endif

typedef struct rt_frame {
    void *ip, *fp, *sp;
} rt_frame;

__attribute__((weak))
int _tcc_backtrace(rt_frame *f, const char *fmt, va_list ap);

DLL_EXPORT int tcc_backtrace(const char *fmt, ...)
{
    va_list ap;
    int ret;

    if (_tcc_backtrace) {
        rt_frame f;
        f.fp = __builtin_frame_address(1);
        f.ip = __builtin_return_address(0);
        va_start(ap, fmt);
        ret = _tcc_backtrace(&f, fmt, ap);
        va_end(ap);
    } else {
        const char *p, *nl = "\n";
        if (fmt[0] == '^' && (p = strchr(fmt + 1, fmt[0])))
            fmt = p + 1;
        if (fmt[0] == '\001')
            ++fmt, nl = "";
        va_start(ap, fmt);
        ret = vfprintf(stderr, fmt, ap);
        va_end(ap);
        fprintf(stderr, "%s", nl), fflush(stderr);
    }
    return ret;
}

#if (defined(__GNUC__) && (__GNUC__ >= 6)) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
