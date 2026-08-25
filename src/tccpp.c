/*
 *  TCC - Tiny C Compiler
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define USING_GLOBALS
#include "tcc.h"

#if defined(_WIN32) && defined(__TINYC__)
  /* allow self-host build with tcc 0.9.27 - doesn't have ldexpl in tcc_libm.h .
   *
   * in tcc 0.9.27 both ldexp and ldexpl are declared in win32/include/math.h,
   * but only ldexp can be linked - via win32/lib/msvcrt.def. we can't test
   * whether we have ldexpl or not, so map ldexpl to ldexp unconditionally.
   *
   * note that ldexpl takes long double while ldexp takes double, however, on
   * windows these types are identical, and current ldexpl in tcc_libm.h also
   * uses the "normal" double scalbn - just like ldexp, so do the same here.
   */
  #undef ldexpl
  #define ldexpl ldexp
#endif

/* #define to 1 to enable (see parse_pp_string()) */
#define ACCEPT_LF_IN_STRINGS 0

/********************************************************/
/* global variables */

ST_DATA int tok_flags;
ST_DATA int parse_flags;

ST_DATA struct BufferedFile *file;
ST_DATA int tok;
ST_DATA CValue tokc;
ST_DATA const int *macro_ptr;
ST_DATA CString tokcstr; /* current parsed string, if any */

/* display benchmark infos */
ST_DATA int tok_ident;
ST_DATA TokenSym **table_ident;
ST_DATA int pp_expr;

/* ------------------------------------------------------------------------- */

static TokenSym *hash_ident[TOK_HASH_SIZE];
static char token_buf[STRING_MAX_SIZE + 1];
static CString cstr_buf;
static TokenString tokstr_buf;
static TokenString unget_buf;
static unsigned char isidnum_table[256 - CH_EOF];
static int pp_debug_tok, pp_debug_symv;
static int pp_counter;
static void tok_print(const int *str, const char *msg, ...);
static void next_nomacro(void);
static void parse_number(const char *p);
static void parse_string(const char *p, int len);

static struct TinyAlloc *toksym_alloc;
static struct TinyAlloc *tokstr_alloc;

static TokenString *macro_stack;

static const char tcc_keywords[] = 
#define DEF(id, str) str "\0"
#include "tcctok.h"
#undef DEF
;

/* WARNING: the content of this string encodes token numbers */
static const unsigned char tok_two_chars[] =
/* outdated -- gr
    "<=\236>=\235!=\225&&\240||\241++\244--\242==\224<<\1>>\2+=\253"
    "-=\255*=\252/=\257%=\245&=\246^=\336|=\374->\313..\250##\266";
*/{
    '<','=', TOK_LE,
    '>','=', TOK_GE,
    '!','=', TOK_NE,
    '&','&', TOK_LAND,
    '|','|', TOK_LOR,
    '+','+', TOK_INC,
    '-','-', TOK_DEC,
    '=','=', TOK_EQ,
    '<','<', TOK_SHL,
    '>','>', TOK_SAR,
    '+','=', TOK_A_ADD,
    '-','=', TOK_A_SUB,
    '*','=', TOK_A_MUL,
    '/','=', TOK_A_DIV,
    '%','=', TOK_A_MOD,
    '&','=', TOK_A_AND,
    '^','=', TOK_A_XOR,
    '|','=', TOK_A_OR,
    '-','>', TOK_ARROW,
    '.','.', TOK_TWODOTS,
    '#','#', TOK_TWOSHARPS,
    0
};

ST_FUNC void skip(int c)
{
    if (tok != c) {
        char tmp[40];
        pstrcpy(tmp, sizeof tmp, get_tok_str(c, &tokc));
        tcc_error("'%s' expected (got '%s')", tmp, get_tok_str(tok, &tokc));
	}
    next();
}

ST_FUNC void expect(const char *msg)
{
    tcc_error("%s expected", msg);
}

/* ------------------------------------------------------------------------- */
/* Custom allocator for tiny objects */

#define USE_TAL

#ifndef USE_TAL /* may cause memory leaks after errors */
#define tal_free(al, p) tcc_free(p)
#define tal_realloc(al, p, size) tcc_realloc(p, size)
#define tal_new(a,b)
#define tal_delete(a)
#else
#if !defined(MEM_DEBUG)
#define tal_free(al, p) tal_free_impl(al, p)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size)
#define TAL_DEBUG_PARAMS
#else
#define TAL_DEBUG MEM_DEBUG
//#define TAL_INFO 1 /* collect and dump allocators stats */
#define tal_free(al, p) tal_free_impl(al, p, __FILE__, __LINE__)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size, __FILE__, __LINE__)
#define TAL_DEBUG_PARAMS , const char *sfile, int sline
#endif

#define TOKSYM_TAL_SIZE (256 * 1024) /* allocator for TokenSym in table_ident */
#define TOKSTR_TAL_SIZE (256 * 1024) /* allocator for TokenString instances */

typedef struct TinyAlloc {
    uint8_t *p;
    uint8_t *bufend;
    struct TinyAlloc *next;
    unsigned nb_allocs;
    unsigned size;
#if TAL_INFO
    unsigned nb_peak;
    unsigned nb_total;
    uint8_t *peak_p;
#endif
    union {
        uint8_t buffer[1];
        size_t _aligner_;
    };
} TinyAlloc;

typedef struct tal_header_t {
    size_t  size; /* word align */
#if TAL_DEBUG
    int     line_num; /* negative line_num used for double free check */
    char    file_name[40];
#endif
} tal_header_t;

#define TAL_ALIGN(size) \
    (((size) + (sizeof (size_t) - 1)) & ~(sizeof (size_t) - 1))

/* ------------------------------------------------------------------------- */

static TinyAlloc *tal_new(TinyAlloc **pal, unsigned size)
{
    TinyAlloc *al = tcc_malloc(sizeof(TinyAlloc) - sizeof (size_t) + size);
    al->p = al->buffer;
    al->bufend = al->buffer + size;
    al->nb_allocs = 0;
    al->next = *pal, *pal = al;
    al->size = al->next ? al->next->size : size;
#if TAL_INFO
    al->nb_peak = 0;
    al->nb_total = 0;
    al->peak_p = al->p;
#endif
    return al;
}

static void tal_delete(TinyAlloc **pal)
{
    TinyAlloc *al = *pal, *next;

#if TAL_INFO
    fprintf(stderr, "tal_delete (&tok%s_alloc):\n", pal == &toksym_alloc ? "sym" : "str");
#endif
tail_call:
#if TAL_DEBUG && TAL_DEBUG != 3 /* do not check TAL leaks with -DMEM_DEBUG=3 */
#if TAL_INFO
    fprintf(stderr, "  size %7d  nb_peak %5d  nb_total %6d  usage %5.1f%%\n",
            al->bufend - al->buffer, al->nb_peak, al->nb_total,
            (al->peak_p - al->buffer) * 100.0 / (al->bufend - al->buffer));
#endif
    if (al->nb_allocs > 0) {
        uint8_t *p;
        fprintf(stderr, "TAL_DEBUG: memory leak %d chunk(s)\n", al->nb_allocs);
        p = al->buffer;
        while (p < al->p) {
            tal_header_t *header = (tal_header_t *)p;
            if (header->line_num > 0) {
                fprintf(stderr, "%s:%d: chunk of %d bytes leaked\n",
                        header->file_name, header->line_num, (int)header->size);
            }
            p += header->size + sizeof(tal_header_t);
        }
#if TAL_DEBUG == 2
        exit(2);
#endif
    }
#endif
    next = al->next;
    tcc_free(al);
    al = next;
    if (al)
        goto tail_call;
    *pal = al;
}

static void tal_free_impl(TinyAlloc **pal, void *p TAL_DEBUG_PARAMS)
{
    TinyAlloc *al, **top = pal;
    tal_header_t *header;

    if (!p)
        return;
    header = (tal_header_t *)p - 1;
#if TAL_DEBUG
    if (header->line_num < 0) {
        fprintf(stderr, "%s:%d: TAL_DEBUG: double frees chunk from\n",
                sfile, sline);
        fprintf(stderr, "%s:%d: %d bytes\n",
                header->file_name, (int)-header->line_num, (int)header->size);
    } else
        header->line_num = -header->line_num;
#endif
    al = *pal;
    while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
        al = *(pal = &al->next);
    if (0 == --al->nb_allocs) {
        *pal = al->next;
        if ((al->bufend - al->buffer) > al->size) {
            //fprintf(stderr, "free big tal: %u\n", header->size);
            tcc_free(al);
        } else {
            /* reset and move to front */
            al->p = al->buffer;
            al->next = *top, *top = al;
        }
    } else if ((uint8_t*)p + header->size == al->p) {
        al->p = (uint8_t*)header;
    }
}

static void *tal_realloc_impl(TinyAlloc **pal, void *p, unsigned size TAL_DEBUG_PARAMS)
{
    tal_header_t *header;
    void *ret;
    unsigned adj_size = TAL_ALIGN(size) + sizeof(tal_header_t);
    TinyAlloc *al = *pal;

    if (p) {
        /* reallpc case */
        while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
            al = al->next;
        header = (tal_header_t *)p - 1;
        if ((uint8_t*)p + header->size == al->p)
            al->p = (uint8_t*)header; /* maybe reuse */
        if (al->p + adj_size > al->bufend) {
            ret = tal_realloc(pal, 0, size);
            memcpy(ret, p, header->size);
            tal_free(pal, p);
            return ret;
        } else if (al->p != (uint8_t*)header) {
            memcpy((tal_header_t*)al->p + 1, p, header->size);
#if TAL_DEBUG
            header->line_num = -header->line_num;
#endif
        }
    } else {
        /* new alloc case */
        while (al->p + adj_size > al->bufend) {
            al = al->next;
            if (!al) {
                unsigned new_size = (*pal)->size;
                if (adj_size > new_size) {
                    new_size = adj_size;
                    //fprintf(stderr, "%s:%d: alloc big tal: %u\n", file->filename, file->line_num, adj_size - sizeof(tal_header_t));
                }
                al = tal_new(pal, new_size);
                break;
            }
        }
        al->nb_allocs++;
    }
    header = (tal_header_t *)al->p;
    header->size = adj_size - sizeof(tal_header_t);
    al->p += adj_size;
    ret = header + 1;
#if  TAL_DEBUG
    {
        int ofs = strlen(sfile) + 1 - sizeof header->file_name;
        strcpy(header->file_name, sfile + (ofs > 0 ? ofs : 0));
        header->line_num = sline;
#if TAL_INFO
        if (al->nb_peak < al->nb_allocs)
            al->nb_peak = al->nb_allocs;
        if (al->peak_p < al->p)
            al->peak_p = al->p;
        al->nb_total++;
#endif
    }
#endif
    return ret;
}

#endif /* USE_TAL */

/* ------------------------------------------------------------------------- */
/* CString handling */
static void cstr_realloc(CString *cstr, int new_size)
{
    int size;

    size = cstr->size_allocated;
    if (size < 8)
        size = 8; /* no need to allocate a too small first string */
    while (size < new_size)
        size = size * 2;
    cstr->data = tcc_realloc(cstr->data, size);
    cstr->size_allocated = size;
}

/* add a byte */
ST_INLN void cstr_ccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + 1;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    cstr->data[size - 1] = ch;
    cstr->size = size;
}

ST_INLN char *unicode_to_utf8 (char *b, uint32_t Uc)
{
    if (Uc<0x80) *b++=Uc;
    else if (Uc<0x800) *b++=192+Uc/64, *b++=128+Uc%64;
    else if (Uc-0xd800u<0x800) goto error;
    else if (Uc<0x10000) *b++=224+Uc/4096, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else if (Uc<0x110000) *b++=240+Uc/262144, *b++=128+Uc/4096%64, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else error: tcc_error("0x%x is not a valid universal character", Uc);
    return b;
}

/* add a unicode character expanded into utf8 */
ST_INLN void cstr_u8cat(CString *cstr, int ch)
{
    char buf[4], *e;
    e = unicode_to_utf8(buf, (uint32_t)ch);
    cstr_cat(cstr, buf, e - buf);
}

/* add string of 'len', or of its len/len+1 when 'len' == -1/0 */
ST_FUNC void cstr_cat(CString *cstr, const char *str, int len)
{
    int size;
    if (len <= 0)
        len = strlen(str) + 1 + len;
    size = cstr->size + len;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    memmove(cstr->data + cstr->size, str, len);
    cstr->size = size;
}

/* add a wide char */
ST_FUNC void cstr_wccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + sizeof(nwchar_t);
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    *(nwchar_t *)(cstr->data + size - sizeof(nwchar_t)) = ch;
    cstr->size = size;
}

ST_FUNC void cstr_new(CString *cstr)
{
    memset(cstr, 0, sizeof(CString));
}

/* free string and reset it to NULL */
ST_FUNC void cstr_free(CString *cstr)
{
    tcc_free(cstr->data);
}

/* reset string to empty */
ST_FUNC void cstr_reset(CString *cstr)
{
    cstr->size = 0;
}

ST_FUNC int cstr_vprintf(CString *cstr, const char *fmt, va_list ap)
{
    va_list v;
    int len, size = 80;
    for (;;) {
        size += cstr->size;
        if (size > cstr->size_allocated)
            cstr_realloc(cstr, size);
        size = cstr->size_allocated - cstr->size;
        va_copy(v, ap);
        len = vsnprintf(cstr->data + cstr->size, size, fmt, v);
        va_end(v);
        if (len >= 0 && len < size)
            break;
        size *= 2;
    }
    cstr->size += len;
    return len;
}

ST_FUNC int cstr_printf(CString *cstr, const char *fmt, ...)
{
    va_list ap; int len;
    va_start(ap, fmt);
    len = cstr_vprintf(cstr, fmt, ap);
    va_end(ap);
    return len;
}

/* XXX: unicode ? */
static void add_char(CString *cstr, int c)
{
    if (c == '\'' || c == '\"' || c == '\\') {
        /* XXX: could be more precise if char or string */
        cstr_ccat(cstr, '\\');
    }
    if (c >= 32 && c <= 126) {
        cstr_ccat(cstr, c);
    } else {
        cstr_ccat(cstr, '\\');
        if (c == '\n') {
            cstr_ccat(cstr, 'n');
        } else {
            cstr_ccat(cstr, '0' + ((c >> 6) & 7));
            cstr_ccat(cstr, '0' + ((c >> 3) & 7));
            cstr_ccat(cstr, '0' + (c & 7));
        }
    }
}

/* ------------------------------------------------------------------------- */
/* allocate a new token */
static TokenSym *tok_alloc_new(TokenSym **pts, const char *str, int len)
{
    TokenSym *ts, **ptable;
    int i;

    if (tok_ident >= SYM_FIRST_ANOM) 
        tcc_error("memory full (symbols)");

    /* expand token table if needed */
    i = tok_ident - TOK_IDENT;
    if ((i % TOK_ALLOC_INCR) == 0) {
        ptable = tcc_realloc(table_ident, (i + TOK_ALLOC_INCR) * sizeof(TokenSym *));
        table_ident = ptable;
    }

    ts = tal_realloc(&toksym_alloc, 0, sizeof(TokenSym) + len);
    table_ident[i] = ts;
    ts->tok = tok_ident++;
    ts->sym_define = NULL;
    ts->sym_label = NULL;
    ts->sym_struct = NULL;
    ts->sym_identifier = NULL;
    ts->len = len;
    ts->hash_next = NULL;
    memcpy(ts->str, str, len);
    ts->str[len] = '\0';
    *pts = ts;
    return ts;
}

#define TOK_HASH_INIT 1
#define TOK_HASH_FUNC(h, c) ((h) + ((h) << 5) + ((h) >> 27) + (c))


/* find a token and add it if not found */
ST_FUNC TokenSym *tok_alloc(const char *str, int len)
{
    TokenSym *ts, **pts;
    int i;
    unsigned int h;
    
    h = TOK_HASH_INIT;
    for(i=0;i<len;i++)
        h = TOK_HASH_FUNC(h, ((unsigned char *)str)[i]);
    h &= (TOK_HASH_SIZE - 1);

    pts = &hash_ident[h];
    for(;;) {
        ts = *pts;
        if (!ts)
            break;
        if (ts->len == len && !memcmp(ts->str, str, len))
            return ts;
        pts = &(ts->hash_next);
    }
    return tok_alloc_new(pts, str, len);
}

ST_FUNC int tok_alloc_const(const char *str)
{
    return tok_alloc(str, strlen(str))->tok;
}


/* XXX: buffer overflow */
/* XXX: float tokens */
ST_FUNC const char *get_tok_str(int v, CValue *cv)
{
    char *p;
    int i, len;

    cstr_reset(&cstr_buf);
    p = cstr_buf.data;

    switch(v) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CLONG:
    case TOK_CULONG:
    case TOK_CLLONG:
    case TOK_CULLONG:
        /* XXX: not quite exact, but only useful for testing  */
        sprintf(p, "%llu", (unsigned long long)cv->i);
        break;
    case TOK_LCHAR:
        cstr_ccat(&cstr_buf, 'L');
    case TOK_CCHAR:
        cstr_ccat(&cstr_buf, '\'');
        add_char(&cstr_buf, cv->i);
        cstr_ccat(&cstr_buf, '\'');
        cstr_ccat(&cstr_buf, '\0');
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
        return (char*)cv->str.data;
    case TOK_LSTR:
        cstr_ccat(&cstr_buf, 'L');
    case TOK_STR:
        cstr_ccat(&cstr_buf, '\"');
        if (v == TOK_STR) {
            len = cv->str.size - 1;
            for(i=0;i<len;i++)
                add_char(&cstr_buf, ((unsigned char *)cv->str.data)[i]);
        } else {
            len = (cv->str.size / sizeof(nwchar_t)) - 1;
            for(i=0;i<len;i++)
                add_char(&cstr_buf, ((nwchar_t *)cv->str.data)[i]);
        }
        cstr_ccat(&cstr_buf, '\"');
        cstr_ccat(&cstr_buf, '\0');
        break;

    case TOK_CFLOAT:
        return strcpy(p, "<float>");
    case TOK_CDOUBLE:
        return strcpy(p, "<double>");
    case TOK_CLDOUBLE:
        return strcpy(p, "<long double>");
    case TOK_LINENUM:
        return strcpy(p, "<linenumber>");

    /* above tokens have value, the ones below don't */
    case TOK_LT:
        v = '<';
        goto addv;
    case TOK_GT:
        v = '>';
        goto addv;
    case TOK_DOTS:
        return strcpy(p, "...");
    case TOK_A_SHL:
        return strcpy(p, "<<=");
    case TOK_A_SAR:
        return strcpy(p, ">>=");
    case TOK_EOF:
        return strcpy(p, "<eof>");
    case 0: /* anonymous nameless symbols */
        return strcpy(p, "<no name>");
    default:
        v &= ~(SYM_FIELD | SYM_STRUCT);
        if (v < TOK_IDENT) {
            /* search in two bytes table */
            const unsigned char *q = tok_two_chars;
            while (*q) {
                if (q[2] == v) {
                    *p++ = q[0];
                    *p++ = q[1];
                    *p = '\0';
                    return cstr_buf.data;
                }
                q += 3;
            }
            if (v >= 127 || (v < 32 && !is_space(v) && v != '\n')) {
                sprintf(p, "<\\x%02x>", v);
                break;
            }
    addv:
            *p++ = v;
            *p = '\0';
        } else if (v < tok_ident) {
            return table_ident[v - TOK_IDENT]->str;
        } else if (v >= SYM_FIRST_ANOM) {
            /* special name for anonymous symbol */
            sprintf(p, "L.%u", v - SYM_FIRST_ANOM);
        } else {
            /* should never happen */
            return NULL;
        }
        break;
    }
    return cstr_buf.data;
}

/* return the current character, handling end of block if necessary
   (but not stray) */
static int handle_eob(void)
{
    BufferedFile *bf = file;
    int len;

    /* only tries to read if really end of buffer */
    if (bf->buf_ptr >= bf->buf_end) {
        if (bf->fd >= 0) {
#if defined(PARSE_DEBUG)
            len = 1;
#else
            len = IO_BUF_SIZE;
#endif
            len = read(bf->fd, bf->buffer, len);
            if (len < 0)
                len = 0;
        } else {
            len = 0;
        }
        total_bytes += len;
        bf->buf_ptr = bf->buffer;
        bf->buf_end = bf->buffer + len;
        *bf->buf_end = CH_EOB;
    }
    if (bf->buf_ptr < bf->buf_end) {
        return bf->buf_ptr[0];
    } else {
        bf->buf_ptr = bf->buf_end;
        return CH_EOF;
    }
}

/* read next char from current input file and handle end of input buffer */
static int next_c(void)
{
    int ch = *++file->buf_ptr;
    /* end of buffer/file handling */
    if (ch == CH_EOB && file->buf_ptr >= file->buf_end)
        ch = handle_eob();
    return ch;
}

/* input with '\[\r]\n' handling. */
static int handle_stray_noerror(int err)
{
    int ch;
    while ((ch = next_c()) == '\\') {
        ch = next_c();
        if (ch == '\n') {
    newl:
            file->line_num++;
        } else {
            if (ch == '\r') {
                ch = next_c();
                if (ch == '\n')
                    goto newl;
                *--file->buf_ptr = '\r';
            }
            if (err)
                tcc_error("stray '\\' in program");
            /* may take advantage of 'BufferedFile.unget[4}' */
            return *--file->buf_ptr = '\\';
        }
    }
    return ch;
}

#define ninp() handle_stray_noerror(0)

/* handle '\\' in strings, comments and skipped regions */
static int handle_bs(uint8_t **p)
{
    int c;
    file->buf_ptr = *p - 1;
    c = ninp();
    *p = file->buf_ptr;
    return c;
}

/* skip the stray and handle the \\n case. Output an error if
   incorrect char after the stray */
static int handle_stray(uint8_t **p)
{
    int c;
    file->buf_ptr = *p - 1;
    c = handle_stray_noerror(!(parse_flags & PARSE_FLAG_ACCEPT_STRAYS));
    *p = file->buf_ptr;
    return c;
}

/* handle the complicated stray case */
#define PEEKC(c, p)\
{\
    c = *++p;\
    if (c == '\\')\
        c = handle_stray(&p); \
}

static int skip_spaces(void)
{
    int ch;
    --file->buf_ptr;
    do {
        ch = ninp();
    } while (isidnum_table[ch - CH_EOF] & IS_SPC);
    return ch;
}

/* single line C++ comments */
static uint8_t *parse_line_comment(uint8_t *p)
{
    int c;
    for(;;) {
        for (;;) {
            c = *++p;
    redo:
            if (c == '\n' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '\\')
                break;
        }
        if (c == '\n')
            break;
        c = handle_bs(&p);
        if (c == CH_EOF)
            break;
        if (c != '\\')
            goto redo;
    }
    return p;
}

/* C comments */
static uint8_t *parse_comment(uint8_t *p)
{
    int c;
    for(;;) {
        /* fast skip loop */
        for(;;) {
            c = *++p;
        redo:
            if (c == '\n' || c == '*' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '*' || c == '\\')
                break;
        }
        /* now we can handle all the cases */
        if (c == '\n') {
            file->line_num++;
        } else if (c == '*') {
            do {
                c = *++p;
            } while (c == '*');
            if (c == '\\')
                c = handle_bs(&p);
            if (c == '/')
                break;
            goto check_eof;
        } else {
            c = handle_bs(&p);
        check_eof:
            if (c == CH_EOF)
                tcc_error("unexpected end of file in comment");
            if (c != '\\')
                goto redo;
        }
    }
    return p + 1;
}

/* parse a string without interpreting escapes */
static uint8_t *parse_pp_string(uint8_t *p, int sep, CString *str)
{
    int c;
    for(;;) {
        c = *++p;
    redo:
        if (c == sep) {
            break;
        } else if (c == '\\') {
            c = handle_bs(&p);
            if (c == CH_EOF) {
        unterminated_string:
                /* XXX: indicate line number of start of string */
                tok_flags &= ~TOK_FLAG_BOL;
                tcc_error("missing terminating %c character", sep);
            } else if (c == '\\') {
                if (str)
                    cstr_ccat(str, c);
                c = *++p;
                /* add char after '\\' unconditionally */
                if (c == '\\') {
                    c = handle_bs(&p);
                    if (c == CH_EOF)
                        goto unterminated_string;
                }
                goto add_char;
            } else {
                goto redo;
            }
        } else if (c == '\n') {
        add_lf:
            if (ACCEPT_LF_IN_STRINGS) {
                file->line_num++;
                goto add_char;
            } else if (str) { /* not skipping */
                goto unterminated_string;
            } else {
                //tcc_warning("missing terminating %c character", sep);
                return p;
            }
        } else if (c == '\r') {
            c = *++p;
            if (c == '\\')
                c = handle_bs(&p);
            if (c == '\n')
                goto add_lf;
            if (c == CH_EOF)
                goto unterminated_string;
            if (str)
                cstr_ccat(str, '\r');
            goto redo;
        } else {
        add_char:
            if (str)
                cstr_ccat(str, c);
        }
    }
    p++;
    return p;
}

/* skip block of text until #else, #elif or #endif. skip also pairs of
   #if/#endif */
static void preprocess_skip(void)
{
    int a, start_of_line, c, in_warn_or_error;
    uint8_t *p;

    p = file->buf_ptr;
    a = 0;
redo_start:
    start_of_line = 1;
    in_warn_or_error = 0;
    for(;;) {
        c = *p;
        switch(c) {
        case ' ':
        case '\t':
        case '\f':
        case '\v':
        case '\r':
            p++;
            continue;
        case '\n':
            file->line_num++;
            p++;
            goto redo_start;
        case '\\':
            c = handle_bs(&p);
            if (c == CH_EOF)
                expect("#endif");
            if (c == '\\')
                ++p;
            continue;
        /* skip strings */
        case '\"':
        case '\'':
            if (in_warn_or_error)
                goto _default;
            tok_flags &= ~TOK_FLAG_BOL;
            p = parse_pp_string(p, c, NULL);
            break;
        /* skip comments */
        case '/':
            if (in_warn_or_error)
                goto _default;
            ++p;
            c = handle_bs(&p);
            if (c == '*') {
                p = parse_comment(p);
            } else if (c == '/') {
                p = parse_line_comment(p);
            }
            continue;
        case '#':
            p++;
            if (start_of_line) {
                file->buf_ptr = p;
                next_nomacro();
                p = file->buf_ptr;
                if (a == 0 && 
                    (tok == TOK_ELSE || tok == TOK_ELIF || tok == TOK_ENDIF))
                    goto the_end;
                if (tok == TOK_IF || tok == TOK_IFDEF || tok == TOK_IFNDEF)
                    a++;
                else if (tok == TOK_ENDIF)
                    a--;
                else if( tok == TOK_ERROR || tok == TOK_WARNING)
                    in_warn_or_error = 1;
                else if (tok == TOK_LINEFEED)
                    goto redo_start;
                else if (parse_flags & PARSE_FLAG_ASM_FILE)
                    p = parse_line_comment(p - 1);
            }
#if !defined(TCC_TARGET_ARM) && !defined(TCC_TARGET_ARM64)
            else if (parse_flags & PARSE_FLAG_ASM_FILE)
                p = parse_line_comment(p - 1);
#else
            /* ARM/ARM64 assembly uses '#' for constants */
#endif
            break;
_default:
        default:
            p++;
            break;
        }
        start_of_line = 0;
    }
 the_end: ;
    file->buf_ptr = p;
}

#if 0
/* return the number of additional 'ints' necessary to store the
   token */
static inline int tok_size(const int *p)
{
    switch(*p) {
        /* 4 bytes */
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
        return 1 + 1;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_PPNUM:
    case TOK_PPSTR:
        return 1 + 1 + (p[1] + 3) / 4;
    case TOK_CLONG:
    case TOK_CULONG:
	return 1 + LONG_SIZE / 4;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
        return 1 + 2;
    case TOK_CLDOUBLE:
        return 1 + LDOUBLE_WORDS;
    default:
        return 1 + 0;
    }
}
#endif

/* token string handling */
ST_INLN void tok_str_new(TokenString *s)
{
    s->str = NULL;
    s->len = s->need_spc = 0;
    s->allocated_len = 0;
    s->last_line_num = -1;
}

ST_FUNC TokenString *tok_str_alloc(void)
{
    TokenString *str = tal_realloc(&tokstr_alloc, 0, sizeof *str);
    tok_str_new(str);
    return str;
}

ST_FUNC void tok_str_free_str(int *str)
{
    tal_free(&tokstr_alloc, str);
}

ST_FUNC void tok_str_free(TokenString *str)
{
    tok_str_free_str(str->str);
    tal_free(&tokstr_alloc, str);
}

ST_FUNC int *tok_str_realloc(TokenString *s, int new_size)
{
    int *str, size;

    size = s->allocated_len;
    if (size < 16)
        size = 16;
    while (size < new_size)
        size = size * 2;
    if (size > s->allocated_len) {
        str = tal_realloc(&tokstr_alloc, s->str, size * sizeof(int));
        s->allocated_len = size;
        s->str = str;
    }
    return s->str;
}

ST_FUNC void tok_str_add(TokenString *s, int t)
{
    int len, *str;

    len = s->len;
    str = s->str;
    if (len >= s->allocated_len)
        str = tok_str_realloc(s, len + 1);
    str[len++] = t;
    s->len = len;
}

ST_FUNC void begin_macro(TokenString *str, int alloc)
{
    str->alloc = alloc;
    str->prev = macro_stack;
    str->prev_ptr = macro_ptr;
    str->save_line_num = file->line_num;
    macro_ptr = str->str;
    macro_stack = str;
}

ST_FUNC void end_macro(void)
{
    TokenString *str = macro_stack;
    macro_stack = str->prev;
    macro_ptr = str->prev_ptr;
    file->line_num = str->save_line_num;
    if (str->alloc == 0) {
        /* matters if str not alloced, may be tokstr_buf */
        str->len = str->need_spc = 0;
    } else {
        if (str->alloc == 2)
            str->str = NULL; /* don't free */
        tok_str_free(str);
    }
}

static void tok_str_add2(TokenString *s, int t, CValue *cv)
{
    int len, *str;

    len = s->len;
    str = s->str;

    /* allocate space for worst case */
    if (len + TOK_MAX_SIZE >= s->allocated_len)
        str = tok_str_realloc(s, len + TOK_MAX_SIZE + 1);
    str[len++] = t;
    switch(t) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
#if LONG_SIZE == 4
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        str[len++] = cv->tab[0];
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
    case TOK_STR:
    case TOK_LSTR:
        {
            /* Insert the string into the int array. */
            size_t nb_words =
                1 + (cv->str.size + sizeof(int) - 1) / sizeof(int);
            if (len + nb_words >= s->allocated_len)
                str = tok_str_realloc(s, len + nb_words + 1);
            str[len] = cv->str.size;
            memcpy(&str[len + 1], cv->str.data, cv->str.size);
            len += nb_words;
        }
        break;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
#if LONG_SIZE == 8
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        break;
    case TOK_CLDOUBLE:
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        if (LDOUBLE_WORDS >= 3)
        str[len++] = cv->tab[2];
        if (LDOUBLE_WORDS >= 4)
        str[len++] = cv->tab[3];
    default:
        break;
    }
    s->len = len;
}

/* add the current parse token in token string 's' */
ST_FUNC void tok_str_add_tok(TokenString *s)
{
    CValue cval;

    /* save line number info */
    if (file->line_num != s->last_line_num) {
        s->last_line_num = file->line_num;
        cval.i = s->last_line_num;
        tok_str_add2(s, TOK_LINENUM, &cval);
    }
    tok_str_add2(s, tok, &tokc);
}

/* like tok_str_add2(), add a space if needed */
static void tok_str_add2_spc(TokenString *s, int t, CValue *cv)
{
    if (s->need_spc == 3)
        tok_str_add(s, ' ');
    s->need_spc = 2;
    tok_str_add2(s, t, cv);
}

/* get a token from an integer array and increment pointer. */
static inline void tok_get(int *t, const int **pp, CValue *cv)
{
    const int *p = *pp;
    int n, *tab;

    tab = cv->tab;
    switch(*t = *p++) {
#if LONG_SIZE == 4
    case TOK_CLONG:
#endif
    case TOK_CINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_LINENUM:
        cv->i = *p++;
        break;
#if LONG_SIZE == 4
    case TOK_CULONG:
#endif
    case TOK_CUINT:
        cv->i = (unsigned)*p++;
        break;
    case TOK_CFLOAT:
	tab[0] = *p++;
	break;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_PPNUM:
    case TOK_PPSTR:
        cv->str.size = *p++;
        cv->str.data = (char*)p;
        p += (cv->str.size + sizeof(int) - 1) / sizeof(int);
        break;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
#if LONG_SIZE == 8
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        n = 2;
        goto copy;
    case TOK_CLDOUBLE:
        n = LDOUBLE_WORDS;
    copy:
        do
            *tab++ = *p++;
        while (--n);
        break;
    default:
        break;
    }
    *pp = p;
}

#if 0
# define TOK_GET(t,p,c) tok_get(t,p,c)
#else
# define TOK_GET(t,p,c) do { \
    int _t = **(p); \
    if (TOK_HAS_VALUE(_t)) \
        tok_get(t, p, c); \
    else \
        *(t) = _t, ++*(p); \
    } while (0)
#endif

static int macro_is_equal(const int *a, const int *b)
{
    CValue cv;
    int t;

    if (!a || !b)
        return 1;

    while (*a && *b) {
        cstr_reset(&tokcstr);
        TOK_GET(&t, &a, &cv);
        cstr_cat(&tokcstr, get_tok_str(t, &cv), 0);
        TOK_GET(&t, &b, &cv);
        if (strcmp(tokcstr.data, get_tok_str(t, &cv)))
            return 0;
    }
    return !(*a || *b);
}

/* defines handling */
ST_INLN void define_push(int v, int macro_type, int *str, Sym *first_arg)
{
    Sym *s, *o;

    o = define_find(v);
    s = sym_push2(&define_stack, v, macro_type, 0);
    s->d = str;
    s->next = first_arg;
    table_ident[v - TOK_IDENT]->sym_define = s;

    if (o && !macro_is_equal(o->d, s->d))
	tcc_warning("%s redefined", get_tok_str(v, NULL));
}

/* undefined a define symbol. Its name is just set to zero */
ST_FUNC void define_undef(Sym *s)
{
    int v = s->v;
    if (v >= TOK_IDENT && v < tok_ident)
        table_ident[v - TOK_IDENT]->sym_define = NULL;
}

ST_INLN Sym *define_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_define;
}

/* free define stack until top reaches 'b' */
ST_FUNC void free_defines(Sym *b)
{
    while (define_stack != b) {
        Sym *top = define_stack;
        define_stack = top->prev;
        tok_str_free_str(top->d);
        define_undef(top);
        sym_free(top);
    }
}

/* fake the nth "#if defined test_..." for tcc -dt -run */
static void maybe_run_test(TCCState *s)
{
    const char *p;
    if (s->include_stack_ptr != s->include_stack)
        return;
    p = get_tok_str(tok, NULL);
    if (0 != memcmp(p, "test_", 5))
        return;
    if (0 != --s->run_test)
        return;
    fprintf(s->ppfp, &"\n[%s]\n"[!(s->dflag & 32)], p), fflush(s->ppfp);
    define_push(tok, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void skip_to_eol(int warn)
{
    if (tok == TOK_LINEFEED)
        return;
    if (warn)
        tcc_warning("extra tokens after directive");
    while (macro_stack)
        end_macro();
    file->buf_ptr = parse_line_comment(file->buf_ptr - 1);
    next_nomacro();
}

static CachedInclude *
search_cached_include(TCCState *s1, const char *filename, int add);

static int parse_include(TCCState *s1, int do_next, int test)
{
    int c, i;
    char name[1024], buf[1024], *p;
    CachedInclude *e;

    c = skip_spaces();
    if (c == '<' || c == '\"') {
        cstr_reset(&tokcstr);
        file->buf_ptr = parse_pp_string(file->buf_ptr, c == '<' ? '>' : c, &tokcstr);
        i = tokcstr.size;
        pstrncpy(name, sizeof name, tokcstr.data, i);
        next_nomacro();
    } else {
        /* computed #include : concatenate tokens until result is one of
           the two accepted forms.  Don't convert pp-tokens to tokens here. */
	parse_flags = PARSE_FLAG_PREPROCESS
                    | PARSE_FLAG_LINEFEED
                    | (parse_flags & PARSE_FLAG_ASM_FILE);
        name[0] = 0;
        for (;;) {
            next();
            p = name, i = strlen(p) - 1;
            if (i > 0
                && ((p[0] == '"' && p[i] == '"')
                 || (p[0] == '<' && p[i] == '>')))
                break;
            if (tok == TOK_LINEFEED)
                tcc_error("'#include' expects \"FILENAME\" or <FILENAME>");
            pstrcat(name, sizeof name, get_tok_str(tok, &tokc));
	}
        c = p[0];
        /* remove '<>|""' */
        memmove(p, p + 1, i - 1), p[i - 1] = 0;
    }

    if (!test && s1->output_type == TCC_OUTPUT_DESUGAR) {
        /* 脱糖 include 策略:
         *  - 系统头 (<...>) 或 #include_next: 保留 #include 指令本身, 不展开 → 产物交
         *    clang 时用其自身 sysroot 重新解析, 避免 tcc 私有类型/绝对路径泄漏.
         *  - 项目引号头 ("...", 如 lib/stl/*.h): 递归展开进本 TU —— 这些头用 model/
         *    operator 等扩展语法, clang 无法直接解析, 必须 inline 其内容让脱糖把
         *    模板落地为具体类型/函数(其内部对 <...> 系统头仍保留 include). */
        if (c == '<' || do_next) {
            char d = (c == '<') ? '>' : '"';
            if (!do_next) /* #include_next 顺序敏感, 脱糖不保留 */
                fprintf(s1->ppfp, "#include %c%s%c\n", c, name, d);
            return 0;
        }
        /* c == '"' 且非 #include_next: fall through 到下方常规 tcc_open 展开 */
    }

    if (!test)
        skip_to_eol(1);

    i = do_next ? file->include_next_index : -1;
    for (;;) {
        ++i;
        if (i == 0) {
            /* check absolute include path */
            if (!IS_ABSPATH(name))
                continue;
            buf[0] = '\0';
        } else if (i == 1) {
            /* search in file's dir if "header.h" */
            if (c != '\"')
                continue;
            p = file->true_filename;
            pstrncpy(buf, sizeof buf, p, tcc_basename(p) - p);
        } else {
            int j = i - 2, k = j - s1->nb_include_paths;
            if (k < 0)
                p = s1->include_paths[j];
            else if (k < s1->nb_sysinclude_paths)
                p = s1->sysinclude_paths[k];
            else if (test)
                return 0;
            else
                tcc_error("include file '%s' not found", name);
            pstrcpy(buf, sizeof buf, p);
            pstrcat(buf, sizeof buf, "/");
        }
        pstrcat(buf, sizeof buf, name);
        e = search_cached_include(s1, buf, 0);
        if (e && (define_find(e->ifndef_macro) || e->once)) {
            /* no need to parse the include because the 'ifndef macro'
               is defined (or had #pragma once) */
#ifdef INC_DEBUG
            printf("%s: skipping cached %s\n", file->filename, buf);
#endif
            if ((s1->verbose | 1) == 3) /* -vv[v] */
                printf("=> %*s%s\n",
                   (int)(s1->include_stack_ptr - s1->include_stack), "", buf);
            return 1;
        }
        if (tcc_open(s1, buf) >= 0)
            break;
    }

    if (test) {
        tcc_close();
    } else {
        if (s1->include_stack_ptr >= s1->include_stack + INCLUDE_STACK_SIZE)
            tcc_error("#include recursion too deep");
        /* push previous file on stack */
        *s1->include_stack_ptr++ = file->prev;
        file->include_next_index = i;
#ifdef INC_DEBUG
        printf("%s: including %s\n", file->prev->filename, file->filename);
#endif
        /* update target deps */
        if (s1->gen_deps) {
            BufferedFile *bf = file;
            while (i == 1 && (bf = bf->prev))
                i = bf->include_next_index;
            /* skip system include files */
            if (s1->include_sys_deps || i - 2 < s1->nb_include_paths)
                dynarray_add(&s1->target_deps, &s1->nb_target_deps,
                    tcc_strdup(buf));
        }
        /* add include file debug info */
        tcc_debug_bincl(s1);
    }
    return 1;
}

/* eval an expression for #if/#elif */
static int expr_preprocess(TCCState *s1)
{
    int c, t;
    int t0 = tok;
    TokenString *str;
    
    str = tok_str_alloc();
    pp_expr = 1;
    while (1) {
        next(); /* do macro subst */
        t = tok;
        if (tok < TOK_IDENT) {
            if (tok == TOK_LINEFEED || tok == TOK_EOF)
                break;
            if (tok >= TOK_STR && tok <= TOK_CLDOUBLE)
                tcc_error("invalid constant in preprocessor expression");

        } else if (tok == TOK_DEFINED) {
            parse_flags &= ~PARSE_FLAG_PREPROCESS; /* no macro subst */
            next();
            t = tok;
            if (t == '(') 
                next();
            parse_flags |= PARSE_FLAG_PREPROCESS;
            if (tok < TOK_IDENT)
                expect("identifier after 'defined'");
            if (s1->run_test)
                maybe_run_test(s1);
            c = 0;
            if (define_find(tok)
                || tok == TOK___HAS_INCLUDE
                || tok == TOK___HAS_INCLUDE_NEXT)
                c = 1;
            if (t == '(') {
                next();
                if (tok != ')')
                    expect("')'");
            }
            goto c_number;
        } else if (tok == TOK___HAS_INCLUDE ||
                   tok == TOK___HAS_INCLUDE_NEXT) {
            t = tok;
            next();
	    if (tok != '(')
		expect("'('");
            c = parse_include(s1, t - TOK___HAS_INCLUDE, 1);
            if (tok != ')')
                expect("')'");
            goto c_number;
        } else {
            /* if undefined macro, replace with zero */
            c = 0;
        c_number:
            tok = TOK_CLLONG; /* type intmax_t */
            tokc.i = c;
        }
        tok_str_add_tok(str);
    }
    if (0 == str->len)
        tcc_error("#%s with no expression", get_tok_str(t0, 0));
    tok_str_add(str, TOK_EOF); /* simulate end of file */
    pp_expr = t0; /* redirect pre-processor expression error messages */
    t = tok;
    /* now evaluate C constant expression */
    begin_macro(str, 1);
    next();
    c = expr_const();
    if (tok != TOK_EOF)
        tcc_error("...");
    pp_expr = 0;
    end_macro();
    tok = t; /* restore LF or EOF */
    return c != 0;
}

ST_FUNC void pp_error(CString *cs)
{
    cstr_printf(cs, "bad preprocessor expression: #%s", get_tok_str(pp_expr, 0));
    macro_ptr = macro_stack->str;
    while (next(), tok != TOK_EOF)
        cstr_printf(cs, " %s", get_tok_str(tok, &tokc));
}

/* parse after #define */
ST_FUNC void parse_define(void)
{
    Sym *s, *first, **ps;
    int v, t, varg, is_vaargs, t0;
    int saved_parse_flags = parse_flags;
    TokenString str;

    v = tok;
    if (v < TOK_IDENT || v == TOK_DEFINED)
        tcc_error("invalid macro name '%s'", get_tok_str(tok, &tokc));
    first = NULL;
    t = MACRO_OBJ;
    /* We have to parse the whole define as if not in asm mode, in particular
       no line comment with '#' must be ignored.  Also for function
       macros the argument list must be parsed without '.' being an ID
       character.  */
    parse_flags = ((parse_flags & ~PARSE_FLAG_ASM_FILE) | PARSE_FLAG_SPACES);
    /* '(' must be just after macro definition for MACRO_FUNC */
    next_nomacro();
    parse_flags &= ~PARSE_FLAG_SPACES;
    is_vaargs = 0;
    if (tok == '(') {
        int dotid = set_idnum('.', 0);
        next_nomacro();
        ps = &first;
        if (tok != ')') for (;;) {
            varg = tok;
            next_nomacro();
            is_vaargs = 0;
            if (varg == TOK_DOTS) {
                varg = TOK___VA_ARGS__;
                is_vaargs = 1;
            } else if (tok == TOK_DOTS && gnu_ext) {
                is_vaargs = 1;
                next_nomacro();
            }
            if (varg < TOK_IDENT)
        bad_list:
                tcc_error("bad macro parameter list");
            s = sym_push2(&define_stack, varg | SYM_FIELD, is_vaargs, 0);
            *ps = s;
            ps = &s->next;
            if (tok == ')')
                break;
            if (tok != ',' || is_vaargs)
                goto bad_list;
            next_nomacro();
        }
        parse_flags |= PARSE_FLAG_SPACES;
        next_nomacro();
        t = MACRO_FUNC;
        set_idnum('.', dotid);
    }

    /* The body of a macro definition should be parsed such that identifiers
       are parsed like the file mode determines (i.e. with '.' being an
       ID character in asm mode).  But '#' should be retained instead of
       regarded as line comment leader, so still don't set ASM_FILE
       in parse_flags. */
    parse_flags |= PARSE_FLAG_ACCEPT_STRAYS | PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED;
    tok_str_new(&str);
    t0 = 0;
    while (tok != TOK_LINEFEED && tok != TOK_EOF) {
        if (is_space(tok)) {
            str.need_spc |= 1;
        } else {
            if (TOK_TWOSHARPS == tok) {
                if (0 == t0)
                    goto bad_twosharp;
                tok = TOK_PPJOIN;
                t |= MACRO_JOIN;
            }
            tok_str_add2_spc(&str, tok, &tokc);
            t0 = tok;
        }
        next_nomacro();
    }
    parse_flags = saved_parse_flags;
    tok_str_add(&str, 0);
    if (t0 == TOK_PPJOIN)
bad_twosharp:
        tcc_error("'##' cannot appear at either end of macro");
    define_push(v, t, str.str, first);
    //tok_print(str.str, "#define (%d) %s %d:", t | is_vaargs * 4, get_tok_str(v, 0));
}

static CachedInclude *search_cached_include(TCCState *s1, const char *filename, int add)
{
    const char *s, *basename;
    unsigned int h;
    CachedInclude *e;
    int c, i, len;

    s = basename = tcc_basename(filename);
    h = TOK_HASH_INIT;
    while ((c = (unsigned char)*s) != 0) {
#ifdef _WIN32
        h = TOK_HASH_FUNC(h, toup(c));
#else
        h = TOK_HASH_FUNC(h, c);
#endif
        s++;
    }
    h &= (CACHED_INCLUDES_HASH_SIZE - 1);

    i = s1->cached_includes_hash[h];
    for(;;) {
        if (i == 0)
            break;
        e = s1->cached_includes[i - 1];
        if (0 == PATHCMP(filename, e->filename))
            return e;
        if (e->once
            && 0 == PATHCMP(basename, tcc_basename(e->filename))
            && 0 == normalized_PATHCMP(filename, e->filename)
            )
            return e;
        i = e->hash_next;
    }
    if (!add)
        return NULL;

    e = tcc_malloc(sizeof(CachedInclude) + (len = strlen(filename)));
    memcpy(e->filename, filename, len + 1);
    e->ifndef_macro = e->once = 0;
    dynarray_add(&s1->cached_includes, &s1->nb_cached_includes, e);
    /* add in hash table */
    e->hash_next = s1->cached_includes_hash[h];
    s1->cached_includes_hash[h] = s1->nb_cached_includes;
#ifdef INC_DEBUG
    printf("adding cached '%s'\n", filename);
#endif
    return e;
}

static int pragma_parse(TCCState *s1)
{
    next_nomacro();
    if (tok == TOK_push_macro || tok == TOK_pop_macro) {
        int t = tok, v;
        Sym *s;

        if (next(), tok != '(')
            goto pragma_err;
        if (next(), tok != TOK_STR)
            goto pragma_err;
        v = tok_alloc(tokc.str.data, tokc.str.size - 1)->tok;
        if (next(), tok != ')')
            goto pragma_err;
        if (t == TOK_push_macro) {
            while (NULL == (s = define_find(v)))
                define_push(v, 0, NULL, NULL);
            s->type.ref = s; /* set push boundary */
        } else {
            for (s = define_stack; s; s = s->prev)
                if (s->v == v && s->type.ref == s) {
                    s->type.ref = NULL;
                    break;
                }
        }
        if (s)
            table_ident[v - TOK_IDENT]->sym_define = s->d ? s : NULL;
        else
            tcc_warning("unbalanced #pragma pop_macro");
        pp_debug_tok = t, pp_debug_symv = v;

    } else if (tok == TOK_once) {
        search_cached_include(s1, file->true_filename, 1)->once = 1;

    } else if (s1->output_type == TCC_OUTPUT_PREPROCESS
            || s1->output_type == TCC_OUTPUT_DESUGAR) {
        /* tcc -E / --emit-c: keep pragmas below unchanged */
        unget_tok(' ');
        unget_tok(TOK_PRAGMA);
        unget_tok('#');
        unget_tok(TOK_LINEFEED);
        return 1;

    } else if (tok == TOK_pack) {
        /* This may be:
           #pragma pack(1) // set
           #pragma pack() // reset to default
           #pragma pack(push) // push current
           #pragma pack(push,1) // push & set
           #pragma pack(pop) // restore previous */
        next();
        skip('(');
        if (tok == TOK_ASM_pop) {
            next();
            if (s1->pack_stack_ptr <= s1->pack_stack) {
            stk_error:
                tcc_error("out of pack stack");
            }
            s1->pack_stack_ptr--;
        } else {
            int val = 0;
            if (tok != ')') {
                if (tok == TOK_ASM_push) {
                    next();
                    if (s1->pack_stack_ptr >= s1->pack_stack + PACK_STACK_SIZE - 1)
                        goto stk_error;
                    val = *s1->pack_stack_ptr++;
                    if (tok != ',')
                        goto pack_set;
                    next();
                }
                if (tok != TOK_CINT)
                    goto pragma_err;
                val = tokc.i;
                if (val < 1 || val > 16 || (val & (val - 1)) != 0)
                    goto pragma_err;
                next();
            }
        pack_set:
            *s1->pack_stack_ptr = val;
        }
        if (tok != ')')
            goto pragma_err;

    } else if (tok == TOK_comment) {
        char *p; int t;
        next();
        skip('(');
        t = tok;
        next();
        skip(',');
        if (tok != TOK_STR)
            goto pragma_err;
        p = tcc_strdup(tokc.str.data);
        next();
        if (tok != ')')
            goto pragma_err;
        if (t == TOK_lib) {
            dynarray_add(&s1->pragma_libs, &s1->nb_pragma_libs, p);
        } else {
            if (t == TOK_option)
                tcc_set_options(s1, p);
            tcc_free(p);
        }

    } else {
        tcc_warning_c(warn_all)("#pragma %s ignored", get_tok_str(tok, &tokc));
        return 0;
    }
    next();
    return 1;
pragma_err:
    tcc_error("malformed #pragma directive");
}

/* put alternative filename */
ST_FUNC void tccpp_putfile(const char *filename)
{
    char buf[1024];
    buf[0] = 0;
    if (!IS_ABSPATH(filename)) {
        /* prepend directory from real file */
        pstrcpy(buf, sizeof buf, file->true_filename);
        *tcc_basename(buf) = 0;
    }
    pstrcat(buf, sizeof buf, filename);
#ifdef _WIN32
    normalize_slashes(buf);
#endif
    if (0 == strcmp(file->filename, buf))
        return;
    //printf("new file '%s'\n", buf);
    if (file->true_filename == file->filename)
        file->true_filename = tcc_strdup(file->filename);
    pstrcpy(file->filename, sizeof file->filename, buf);
    tcc_debug_newfile(tcc_state);
}

/* is_bof is true if first non space token at beginning of file */
ST_FUNC void preprocess(int is_bof)
{
    TCCState *s1 = tcc_state;
    int c, n, saved_parse_flags;
    char buf[1024], *q;
    Sym *s;

    saved_parse_flags = parse_flags;
    parse_flags = PARSE_FLAG_PREPROCESS
        | PARSE_FLAG_TOK_NUM
        | PARSE_FLAG_TOK_STR
        | PARSE_FLAG_LINEFEED
        | (parse_flags & PARSE_FLAG_ASM_FILE)
        ;

    next_nomacro();
 redo:
    switch(tok) {
    case TOK_DEFINE:
        pp_debug_tok = tok;
        next_nomacro();
        pp_debug_symv = tok;
        parse_define();
        break;
    case TOK_UNDEF:
        pp_debug_tok = tok;
        next_nomacro();
        pp_debug_symv = tok;
        s = define_find(tok);
        /* undefine symbol by putting an invalid name */
        if (s)
            define_undef(s);
        next_nomacro();
        break;
    case TOK_INCLUDE:
    case TOK_INCLUDE_NEXT:
        parse_include(s1, tok - TOK_INCLUDE, 0);
        goto the_end;
    case TOK_IFNDEF:
        c = 1;
        goto do_ifdef;
    case TOK_IF:
        c = expr_preprocess(s1);
        goto do_if;
    case TOK_IFDEF:
        c = 0;
    do_ifdef:
        next_nomacro();
        if (tok < TOK_IDENT)
            tcc_error("invalid argument for '#if%sdef'", c ? "n" : "");
        if (is_bof) {
            if (c) {
#ifdef INC_DEBUG
                printf("#ifndef %s\n", get_tok_str(tok, NULL));
#endif
                file->ifndef_macro = tok;
            }
        }
        if (define_find(tok)
            || tok == TOK___HAS_INCLUDE
            || tok == TOK___HAS_INCLUDE_NEXT)
            c ^= 1;
        next_nomacro();
    do_if:
        if (s1->ifdef_stack_ptr >= s1->ifdef_stack + IFDEF_STACK_SIZE)
            tcc_error("memory full (ifdef)");
        *s1->ifdef_stack_ptr++ = c;
        goto test_skip;
    case TOK_ELSE:
        next_nomacro();
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            tcc_error("#else without matching #if");
        if (s1->ifdef_stack_ptr[-1] & 2)
            tcc_error("#else after #else");
        c = (s1->ifdef_stack_ptr[-1] ^= 3);
        goto test_else;
    case TOK_ELIF:
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            tcc_error("#elif without matching #if");
        c = s1->ifdef_stack_ptr[-1];
        if (c > 1)
            tcc_error("#elif after #else");
        /* last #if/#elif expression was true: we skip */
        if (c == 1) {
            skip_to_eol(0);
            c = 0;
        } else {
            c = expr_preprocess(s1);
            s1->ifdef_stack_ptr[-1] = c;
        }
    test_else:
        if (s1->ifdef_stack_ptr == file->ifdef_stack_ptr + 1)
            file->ifndef_macro = 0;
    test_skip:
        if (!(c & 1)) {
            skip_to_eol(1);
            preprocess_skip();
            is_bof = 0;
            goto redo;
        }
        break;
    case TOK_ENDIF:
        next_nomacro();
        if (s1->ifdef_stack_ptr <= file->ifdef_stack_ptr)
            tcc_error("#endif without matching #if");
        s1->ifdef_stack_ptr--;
        /* '#ifndef macro' was at the start of file. Now we check if
           an '#endif' is exactly at the end of file */
        if (file->ifndef_macro &&
            s1->ifdef_stack_ptr == file->ifdef_stack_ptr) {
            file->ifndef_macro_saved = file->ifndef_macro;
            /* need to set to zero to avoid false matches if another
               #ifndef at middle of file */
            file->ifndef_macro = 0;
            tok_flags |= TOK_FLAG_ENDIF;
        }
        break;

    case TOK_LINE:
        parse_flags &= ~PARSE_FLAG_TOK_NUM;
        next();
        if (tok != TOK_PPNUM) {
    _line_err:
            tcc_error("wrong #line format");
        }
        c = 1;
        goto _line_num;
    case TOK_PPNUM:
        if (parse_flags & PARSE_FLAG_ASM_FILE)
            goto ignore;
        c = 0; /* no error with extra tokens */
    _line_num:
        for (n = 0, q = tokc.str.data; *q; ++q) {
            if (!isnum(*q))
                goto _line_err;
            n = n * 10 + *q - '0';
        }
        parse_flags &= ~PARSE_FLAG_TOK_STR; /* don't parse escape sequences */
        next();
        if (tok != TOK_LINEFEED) {
            if (tok != TOK_PPSTR || tokc.str.data[0] != '"')
                goto _line_err;
            tokc.str.data[tokc.str.size - 2] = 0;
            tccpp_putfile(tokc.str.data + 1);
            next();
            /* skip optional level number & advance to next line */
            skip_to_eol(c);
        }
        if (file->fd > 0)
            total_lines += file->line_num - n;
        file->line_num = n;
        break;

    case TOK_ERROR:
    case TOK_WARNING:
    {
        q = buf;
        c = skip_spaces();
        while (c != '\n' && c != CH_EOF) {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = c;
            c = ninp();
        }
        *q = '\0';
        if (tok == TOK_ERROR)
            tcc_error("#error %s", buf);
        else
            tcc_warning("#warning %s", buf);
        next_nomacro();
        break;
    }
    case TOK_PRAGMA:
        if (!pragma_parse(s1))
            goto ignore;
        break;
    case TOK_LINEFEED:
        goto the_end;
    default:
        /* ignore gas line comment in an 'S' file. */
        if (saved_parse_flags & PARSE_FLAG_ASM_FILE)
            goto ignore;
        if (tok == '!' && is_bof)
            /* '#!' is ignored at beginning to allow C scripts. */
            goto ignore;
        tcc_warning("ignoring unknown preprocessing directive #%s", get_tok_str(tok, &tokc));
    ignore:
        skip_to_eol(0);
        goto the_end;
    }
    skip_to_eol(1);
 the_end:
    parse_flags = saved_parse_flags;
}

/* evaluate escape codes in a string. */
static void parse_escape_string(CString *outstr, const uint8_t *buf, int is_long)
{
    int c, n, i;
    const uint8_t *p;

    p = buf;
    for(;;) {
        c = *p;
        if (c == '\0')
            break;
        if (c == '\\') {
            p++;
            /* escape */
            c = *p;
            switch(c) {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
                /* at most three octal digits */
                n = c - '0';
                p++;
                c = *p;
                if (isoct(c)) {
                    n = n * 8 + c - '0';
                    p++;
                    c = *p;
                    if (isoct(c)) {
                        n = n * 8 + c - '0';
                        p++;
                    }
                }
                c = n;
                goto add_char_nonext;
            case 'x': i = 0; goto parse_hex_or_ucn;
            case 'u': i = 4; goto parse_hex_or_ucn;
            case 'U': i = 8; goto parse_hex_or_ucn;
    parse_hex_or_ucn:
                p++;
                n = 0;
                do {
                    c = *p;
                    if (c >= 'a' && c <= 'f')
                        c = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        c = c - 'A' + 10;
                    else if (isnum(c))
                        c = c - '0';
                    else if (i >= 0)
                        expect("more hex digits in universal-character-name");
                    else
                        goto add_hex_or_ucn;
                    n = (unsigned) n * 16 + c;
                    p++;
                } while (--i);
		if (is_long) {
    add_hex_or_ucn:
                    c = n;
		    goto add_char_nonext;
		}
                cstr_u8cat(outstr, n);
                continue;
            case 'a':
                c = '\a';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case 'e':
                if (!gnu_ext)
                    goto invalid_escape;
                c = 27;
                break;
            case '\'':
            case '\"':
            case '\\': 
            case '?':
                break;
            default:
            invalid_escape:
                if (c >= '!' && c <= '~')
                    tcc_warning("unknown escape sequence: \'\\%c\'", c);
                else
                    tcc_warning("unknown escape sequence: \'\\x%x\'", c);
                break;
            }
        } else if (is_long && c >= 0x80) {
            /* assume we are processing UTF-8 sequence */
            /* reference: The Unicode Standard, Version 10.0, ch3.9 */

            int cont; /* count of continuation bytes */
            int skip; /* how many bytes should skip when error occurred */
            int i;

            /* decode leading byte */
            if (c < 0xC2) {
	            skip = 1; goto invalid_utf8_sequence;
            } else if (c <= 0xDF) {
	            cont = 1; n = c & 0x1f;
            } else if (c <= 0xEF) {
	            cont = 2; n = c & 0xf;
            } else if (c <= 0xF4) {
	            cont = 3; n = c & 0x7;
            } else {
	            skip = 1; goto invalid_utf8_sequence;
            }

            /* decode continuation bytes */
            for (i = 1; i <= cont; i++) {
                int l = 0x80, h = 0xBF;

                /* adjust limit for second byte */
                if (i == 1) {
                    switch (c) {
                    case 0xE0: l = 0xA0; break;
                    case 0xED: h = 0x9F; break;
                    case 0xF0: l = 0x90; break;
                    case 0xF4: h = 0x8F; break;
                    }
                }

                if (p[i] < l || p[i] > h) {
                    skip = i; goto invalid_utf8_sequence;
                }

                n = (n << 6) | (p[i] & 0x3f);
            }

            /* advance pointer */
            p += 1 + cont;
            c = n;
            goto add_char_nonext;

            /* error handling */
        invalid_utf8_sequence:
            tcc_warning("ill-formed UTF-8 subsequence starting with: \'\\x%x\'", c);
            c = 0xFFFD;
            p += skip;
            goto add_char_nonext;

        }
        p++;
    add_char_nonext:
        if (!is_long)
            cstr_ccat(outstr, c);
        else {
#ifdef TCC_TARGET_PE
            /* store as UTF-16 */
            if (c < 0x10000) {
                cstr_wccat(outstr, c);
            } else {
                c -= 0x10000;
                cstr_wccat(outstr, (c >> 10) + 0xD800);
                cstr_wccat(outstr, (c & 0x3FF) + 0xDC00);
            }
#else
            cstr_wccat(outstr, c);
#endif
        }
    }
    /* add a trailing '\0' */
    if (!is_long)
        cstr_ccat(outstr, '\0');
    else
        cstr_wccat(outstr, '\0');
}

static void parse_string(const char *s, int len)
{
    uint8_t buf[1000], *p = buf;
    int is_long, sep;

    if ((is_long = *s == 'L'))
        ++s, --len;
    sep = *s++;
    len -= 2;
    if (len >= sizeof buf)
        p = tcc_malloc(len + 1);
    memcpy(p, s, len);
    p[len] = 0;

    cstr_reset(&tokcstr);
    parse_escape_string(&tokcstr, p, is_long);
    if (p != buf)
        tcc_free(p);

    if (sep == '\'') {
        int char_size, i, n, c;
        /* XXX: make it portable */
        if (!is_long)
            tok = TOK_CCHAR, char_size = 1;
        else
            tok = TOK_LCHAR, char_size = sizeof(nwchar_t);
        n = tokcstr.size / char_size - 1;
        if (n < 1)
            tcc_error("empty character constant");
        if (n > 1)
            tcc_warning_c(warn_all)("multi-character character constant");
        for (c = i = 0; i < n; ++i) {
            if (is_long)
                c = ((nwchar_t *)tokcstr.data)[i];
            else
                c = (c << 8) | ((char *)tokcstr.data)[i];
        }
        tokc.i = c;
    } else {
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        if (!is_long)
            tok = TOK_STR;
        else
            tok = TOK_LSTR;
    }
}

/* we use 128 bit (64/112 needed) numbers */
#define BN_SIZE 4

/* bn = (bn << shift) | or_val */
static int bn_lshift(unsigned int *bn, int shift, int or_val)
{
    int i;
    unsigned int v;
    if (bn[BN_SIZE - 1] >> (32 - shift))
	return shift;
    for(i=0;i<BN_SIZE;i++) {
        v = bn[i];
        bn[i] = (v << shift) | or_val;
        or_val = v >> (32 - shift);
    }
    return 0;
}

static void bn_zero(unsigned int *bn)
{
    int i;
    for(i=0;i<BN_SIZE;i++) {
        bn[i] = 0;
    }
}

/* parse number in null terminated string 'p' and return it in the
   current token */
static void parse_number(const char *p)
{
    int b, t, shift, frac_bits, s, exp_val, ch;
    char *q;
    unsigned int bn[BN_SIZE];
    long double d;

    /* number */
    q = token_buf;
    ch = *p++;
    t = ch;
    ch = *p++;
    *q++ = t;
    b = 10;
    if (t == '.') {
        goto float_frac_parse;
    } else if (t == '0') {
        if (ch == 'x' || ch == 'X') {
            q--;
            ch = *p++;
            b = 16;
        } else if (tcc_state->tcc_ext && (ch == 'b' || ch == 'B')) {
            q--;
            ch = *p++;
            b = 2;
        }
    }
    /* parse all digits. cannot check octal numbers at this stage
       because of floating point constants */
    while (1) {
        if (ch >= 'a' && ch <= 'f')
            t = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F')
            t = ch - 'A' + 10;
        else if (isnum(ch))
            t = ch - '0';
        else
            break;
        if (t >= b)
            break;
        if (q >= token_buf + STRING_MAX_SIZE) {
        num_too_long:
            tcc_error("number too long");
        }
        *q++ = ch;
        ch = *p++;
    }
    if (ch == '.' ||
        ((ch == 'e' || ch == 'E') && b == 10) ||
        ((ch == 'p' || ch == 'P') && (b == 16 || b == 2))) {
        if (b != 10) {
            /* NOTE: strtox should support that for hexa numbers, but
               non ISOC99 libcs do not support it, so we prefer to do
               it by hand */
            /* hexadecimal or binary floats */
            /* XXX: handle overflows */
            frac_bits = 0;
            *q = '\0';
            if (b == 16)
                shift = 4;
            else 
                shift = 1;
            bn_zero(bn);
            q = token_buf;
            while (1) {
                t = *q++;
                if (t == '\0') {
                    break;
                } else if (t >= 'a') {
                    t = t - 'a' + 10;
                } else if (t >= 'A') {
                    t = t - 'A' + 10;
                } else {
                    t = t - '0';
                }
                frac_bits -= bn_lshift(bn, shift, t);
            }
            if (ch == '.') {
                ch = *p++;
                while (1) {
                    t = ch;
                    if (t >= 'a' && t <= 'f') {
                        t = t - 'a' + 10;
                    } else if (t >= 'A' && t <= 'F') {
                        t = t - 'A' + 10;
                    } else if (t >= '0' && t <= '9') {
                        t = t - '0';
                    } else {
                        break;
                    }
                    if (t >= b)
                        tcc_error("invalid digit");
                    frac_bits -= bn_lshift(bn, shift, t);
                    frac_bits += shift;
                    ch = *p++;
                }
            }
            if (ch != 'p' && ch != 'P')
                expect("exponent");
            ch = *p++;
            s = 1;
            exp_val = 0;
            if (ch == '+') {
                ch = *p++;
            } else if (ch == '-') {
                s = -1;
                ch = *p++;
            }
            if (ch < '0' || ch > '9')
                expect("exponent digits");
            while (ch >= '0' && ch <= '9') {
		/* If exp_val is this large ldexp will return HUGE_VAL */
		if (exp_val < 100000000)
                    exp_val = exp_val * 10 + ch - '0';
                ch = *p++;
            }
            exp_val = exp_val * s;

            /* now we can generate the number */
            /* XXX: should patch directly float number */
            d = (long double)bn[3] * 79228162514264337593543950336.0L +
	        (long double)bn[2] * 18446744073709551616.0L +
	        (long double)bn[1] * 4294967296.0L +
	        (long double)bn[0];
            d = ldexpl(d, exp_val - frac_bits);
            t = toup(ch);
            if (t == 'F') {
                ch = *p++;
                tok = TOK_CFLOAT;
                /* float : should handle overflow */
                tokc.f = (float)d;
            } else if (t == 'L') {
                ch = *p++;
                tok = TOK_CLDOUBLE;
                tokc.ld = d;
            } else {
                tok = TOK_CDOUBLE;
                tokc.d = (double)d;
            }
        } else {
            /* decimal floats */
            if (ch == '.') {
                if (q >= token_buf + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
            float_frac_parse:
                while (ch >= '0' && ch <= '9') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            if (ch == 'e' || ch == 'E') {
                if (q >= token_buf + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
                if (ch == '-' || ch == '+') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
                if (ch < '0' || ch > '9')
                    expect("exponent digits");
                while (ch >= '0' && ch <= '9') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            *q = '\0';
            t = toup(ch);
            errno = 0;
            if (t == 'F') {
                ch = *p++;
                tok = TOK_CFLOAT;
                tokc.f = strtof(token_buf, NULL);
            } else if (t == 'L') {
                ch = *p++;
                tok = TOK_CLDOUBLE;
                tokc.ld = strtold(token_buf, NULL);
            } else {
                tok = TOK_CDOUBLE;
                tokc.d = strtod(token_buf, NULL);
            }
        }
    } else {
        unsigned long long n, n1;
        int lcount, ucount, ov = 0;
        const char *p1;

        /* integer number */
        *q = '\0';
        q = token_buf;
        if (b == 10 && *q == '0') {
            b = 8;
            q++;
        }
        n = 0;
        while(1) {
            t = *q++;
            /* no need for checks except for base 10 / 8 errors */
            if (t == '\0')
                break;
            else if (t >= 'a')
                t = t - 'a' + 10;
            else if (t >= 'A')
                t = t - 'A' + 10;
            else
                t = t - '0';
            if (t >= b)
                tcc_error("invalid digit");
            n1 = n;
            n = n * b + t;
            /* detect overflow */
            if (n1 >= 0x1000000000000000ULL && n / b != n1)
                ov = 1;
        }

        /* Determine the characteristics (unsigned and/or 64bit) the type of
           the constant must have according to the constant suffix(es) */
        lcount = ucount = 0;
        p1 = p;
        for(;;) {
            t = toup(ch);
            if (t == 'L') {
                if (lcount >= 2)
                    tcc_error("three 'l's in integer constant");
                if (lcount && *(p - 1) != ch)
                    tcc_error("incorrect integer suffix: %s", p1);
                lcount++;
                ch = *p++;
            } else if (t == 'U') {
                if (ucount >= 1)
                    tcc_error("two 'u's in integer constant");
                ucount++;
                ch = *p++;
            } else {
                break;
            }
        }

        /* in #if/#elif expressions, all numbers have type (u)intmax_t anyway */
        if (pp_expr)
            lcount = 2;

        /* Determine if it needs 64 bits and/or unsigned in order to fit */
        if (ucount == 0 && b == 10) {
            if (lcount <= (LONG_SIZE == 4)) {
                if (n >= 0x80000000U)
                    lcount = (LONG_SIZE == 4) + 1;
            }
            if (n >= 0x8000000000000000ULL)
                ov = 1, ucount = 1;
        } else {
            if (lcount <= (LONG_SIZE == 4)) {
                if (n >= 0x100000000ULL)
                    lcount = (LONG_SIZE == 4) + 1;
                else if (n >= 0x80000000U)
                    ucount = 1;
            }
            if (n >= 0x8000000000000000ULL)
                ucount = 1;
        }

        if (ov)
            tcc_warning("integer constant overflow");

        tok = TOK_CINT;
	if (lcount) {
            tok = TOK_CLONG;
            if (lcount == 2)
                tok = TOK_CLLONG;
	}
	if (ucount)
	    ++tok; /* TOK_CU... */
        tokc.i = n;
    }
    if (ch)
        tcc_error("invalid number");
}


#define PARSE2(c1, tok1, c2, tok2)              \
    case c1:                                    \
        PEEKC(c, p);                            \
        if (c == c2) {                          \
            p++;                                \
            tok = tok2;                         \
        } else {                                \
            tok = tok1;                         \
        }                                       \
        break;

/* return next token without macro substitution */
static void next_nomacro(void)
{
    int t, c, is_long, len;
    TokenSym *ts;
    uint8_t *p, *p1;
    unsigned int h;

    p = file->buf_ptr;
 redo_no_start:
    c = *p;
    switch(c) {
    case ' ':
    case '\t':
        tok = c;
        p++;
 maybe_space:
        if (parse_flags & PARSE_FLAG_SPACES)
            goto keep_tok_flags;
        while (isidnum_table[*p - CH_EOF] & IS_SPC)
            ++p;
        goto redo_no_start;
    case '\f':
    case '\v':
    case '\r':
        p++;
        goto redo_no_start;
    case '\\':
        /* first look if it is in fact an end of buffer */
        c = handle_stray(&p);
        if (c == '\\')
            goto parse_simple;
        if (c == CH_EOF) {
            TCCState *s1 = tcc_state;
            if (!(tok_flags & TOK_FLAG_BOL)) {
                /* add implicit newline */
                goto maybe_newline;
            } else if (!(parse_flags & PARSE_FLAG_PREPROCESS)) {
                tok = TOK_EOF;
            } else if (s1->ifdef_stack_ptr != file->ifdef_stack_ptr) {
                tcc_error("missing #endif");
            } else if (s1->include_stack_ptr == s1->include_stack) {
                /* no include left : end of file. */
                tok = TOK_EOF;
            } else {
                /* pop include file */

                /* test if previous '#endif' was after a #ifdef at
                   start of file */
                if (tok_flags & TOK_FLAG_ENDIF) {
#ifdef INC_DEBUG
                    printf("#endif %s\n", get_tok_str(file->ifndef_macro_saved, NULL));
#endif
                    search_cached_include(s1, file->true_filename, 1)
                        ->ifndef_macro = file->ifndef_macro_saved;
                    tok_flags &= ~TOK_FLAG_ENDIF;
                }

                /* add end of include file debug info */
                tcc_debug_eincl(tcc_state);
                /* pop include stack */
                tcc_close();
                s1->include_stack_ptr--;
                p = file->buf_ptr;
                goto maybe_newline;
            }
        } else {
            goto redo_no_start;
        }
        break;

    case '\n':
        file->line_num++;
        p++;
maybe_newline:
        tok_flags |= TOK_FLAG_BOL;
        if (0 == (parse_flags & PARSE_FLAG_LINEFEED))
            goto redo_no_start;
        tok = TOK_LINEFEED;
        goto keep_tok_flags;

    case '#':
        /* XXX: simplify */
        PEEKC(c, p);
        if ((tok_flags & TOK_FLAG_BOL) && 
            (parse_flags & PARSE_FLAG_PREPROCESS)) {
            tok_flags &= ~TOK_FLAG_BOL;
            file->buf_ptr = p;
            preprocess(tok_flags & TOK_FLAG_BOF);
            p = file->buf_ptr;
            goto maybe_newline;
        } else {
            if (c == '#') {
                p++;
                tok = TOK_TWOSHARPS;
            } else {
#if !defined(TCC_TARGET_ARM) && !defined(TCC_TARGET_ARM64)
                if (parse_flags & PARSE_FLAG_ASM_FILE) {
                    p = parse_line_comment(p - 1);
                    goto redo_no_start;
                } else
#endif
                {
                    tok = '#';
                }
            }
        }
        break;
    
    /* dollar is allowed to start identifiers when not parsing asm */
    case '$':
        if (!(isidnum_table['$' - CH_EOF] & IS_ID)
         || (parse_flags & PARSE_FLAG_ASM_FILE))
            goto parse_simple;

    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z': 
    case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': 
    case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z': 
    case '_':
    parse_ident_fast:
        p1 = p;
        h = TOK_HASH_INIT;
        h = TOK_HASH_FUNC(h, c);
        while (c = *++p, isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
            h = TOK_HASH_FUNC(h, c);
        len = p - p1;
        if (c != '\\') {
            TokenSym **pts;

            /* fast case : no stray found, so we have the full token
               and we have already hashed it */
            h &= (TOK_HASH_SIZE - 1);
            pts = &hash_ident[h];
            for(;;) {
                ts = *pts;
                if (!ts)
                    break;
                if (ts->len == len && !memcmp(ts->str, p1, len))
                    goto token_found;
                pts = &(ts->hash_next);
            }
            ts = tok_alloc_new(pts, (char *) p1, len);
        token_found: ;
        } else {
            /* slower case */
            cstr_reset(&tokcstr);
            cstr_cat(&tokcstr, (char *) p1, len);
            p--;
            PEEKC(c, p);
            while (isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
            {
                cstr_ccat(&tokcstr, c);
                PEEKC(c, p);
            }
            ts = tok_alloc(tokcstr.data, tokcstr.size);
        }
        tok = ts->tok;
        break;
    case 'L':
        t = p[1];
        if (t == '\'' || t == '\"' || t == '\\') {
            PEEKC(c, p);
            if (c == '\'' || c == '\"') {
                is_long = 1;
                goto str_const;
            }
            *--p = c = 'L';
        }
        goto parse_ident_fast;

    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9':
        t = c;
        PEEKC(c, p);
        /* after the first digit, accept digits, alpha, '.' or sign if
           prefixed by 'eEpP' */
    parse_num:
        cstr_reset(&tokcstr);
        for(;;) {
            cstr_ccat(&tokcstr, t);
            if (!((isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
                  || c == '.'
                  || ((c == '+' || c == '-')
                      && (((t == 'e' || t == 'E')
                            && !(parse_flags & PARSE_FLAG_ASM_FILE
                                /* 0xe+1 is 3 tokens in asm */
                                && ((char*)tokcstr.data)[0] == '0'
                                && toup(((char*)tokcstr.data)[1]) == 'X'))
                          || t == 'p' || t == 'P'))))
                break;
            t = c;
            PEEKC(c, p);
        }
        /* We add a trailing '\0' to ease parsing */
        cstr_ccat(&tokcstr, '\0');
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_PPNUM;
        break;

    case '.':
        /* special dot handling because it can also start a number */
        PEEKC(c, p);
        if (isnum(c)) {
            t = '.';
            goto parse_num;
        } else if ((isidnum_table['.' - CH_EOF] & IS_ID)
                   && (isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))) {
            *--p = c = '.';
            goto parse_ident_fast;
        } else if (c == '.') {
            PEEKC(c, p);
            if (c == '.') {
                p++;
                tok = TOK_DOTS;
            } else {
                *--p = '.'; /* may underflow into file->unget[] */
                tok = '.';
            }
        } else {
            tok = '.';
        }
        break;
    case '\'':
    case '\"':
        is_long = 0;
    str_const:
        cstr_reset(&tokcstr);
        if (is_long)
            cstr_ccat(&tokcstr, 'L');
        cstr_ccat(&tokcstr, c);
        p = parse_pp_string(p, c, &tokcstr);
        cstr_ccat(&tokcstr, c);
        cstr_ccat(&tokcstr, '\0');
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_PPSTR;
        break;

    case '<':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_LE;
        } else if (c == '<') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SHL;
            } else {
                tok = TOK_SHL;
            }
        } else {
            tok = TOK_LT;
        }
        break;
    case '>':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_GE;
        } else if (c == '>') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SAR;
            } else {
                tok = TOK_SAR;
            }
        } else {
            tok = TOK_GT;
        }
        break;
        
    case '&':
        PEEKC(c, p);
        if (c == '&') {
            p++;
            tok = TOK_LAND;
        } else if (c == '=') {
            p++;
            tok = TOK_A_AND;
        } else {
            tok = '&';
        }
        break;
        
    case '|':
        PEEKC(c, p);
        if (c == '|') {
            p++;
            tok = TOK_LOR;
        } else if (c == '=') {
            p++;
            tok = TOK_A_OR;
        } else {
            tok = '|';
        }
        break;

    case '+':
        PEEKC(c, p);
        if (c == '+') {
            p++;
            tok = TOK_INC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_ADD;
        } else {
            tok = '+';
        }
        break;
        
    case '-':
        PEEKC(c, p);
        if (c == '-') {
            p++;
            tok = TOK_DEC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_SUB;
        } else if (c == '>') {
            p++;
            tok = TOK_ARROW;
        } else {
            tok = '-';
        }
        break;

    PARSE2('!', '!', '=', TOK_NE)
    PARSE2('=', '=', '=', TOK_EQ)
    PARSE2('*', '*', '=', TOK_A_MUL)
    PARSE2('%', '%', '=', TOK_A_MOD)
    PARSE2('^', '^', '=', TOK_A_XOR)
        
        /* comments or operator */
    case '/':
        PEEKC(c, p);
        if (c == '*') {
            p = parse_comment(p);
            /* comments replaced by a blank */
            tok = ' ';
            goto maybe_space;
        } else if (c == '/') {
            p = parse_line_comment(p);
            tok = ' ';
            goto maybe_space;
        } else if (c == '=') {
            p++;
            tok = TOK_A_DIV;
        } else {
            tok = '/';
        }
        break;
        
        /* simple tokens */
    case '@': /* only used in assembler */
#ifdef TCC_TARGET_ARM /* comment on arm asm */
        if (parse_flags & PARSE_FLAG_ASM_FILE) {
            p = parse_line_comment(p);
            goto redo_no_start;
        }
#endif
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case ',':
    case ';':
    case ':':
    case '?':
    case '~':
    parse_simple:
        tok = c;
        p++;
        break;
    case 0xEF: /* UTF8 BOM ? */
        if (p[1] == 0xBB && p[2] == 0xBF && p == file->buffer) {
            p += 3;
            goto redo_no_start;
        }
    default:
        if (c >= 0x80 && c <= 0xFF) /* utf8 identifiers */
	    goto parse_ident_fast;
        if (parse_flags & PARSE_FLAG_ASM_FILE)
            goto parse_simple;
        tcc_error("unrecognized character \\x%02x", c);
        break;
    }
    tok_flags = 0;
keep_tok_flags:
    file->buf_ptr = p;
#if defined(PARSE_DEBUG)
    printf("token = %d %s\n", tok, get_tok_str(tok, &tokc));
#endif
}

#ifdef PP_DEBUG
static int indent;
static void define_print(TCCState *s1, int v);
static void pp_print(const char *msg, int v, const int *str)
{
    FILE *fp = tcc_state->ppfp;

    if (msg[0] == '#' && indent == 0)
        fprintf(fp, "\n");
    else if (msg[0] == '+')
         ++indent, ++msg;
    else if (msg[0] == '-')
        --indent, ++msg;

    fprintf(fp, "%*s", indent, "");
    if (msg[0] == '#') {
        define_print(tcc_state, v);
    } else {
        tok_print(str, v ? "%s %s" : "%s", msg, get_tok_str(v, 0));
    }
}
#define PP_PRINT(x) pp_print x
#else
#define PP_PRINT(x)
#endif

static int macro_subst(
    TokenString *tok_str,
    Sym **nested_list,
    const int *macro_str
    );

/* substitute arguments in replacement lists in macro_str by the values in
   args (field d) and return allocated string */
static int *macro_arg_subst(Sym **nested_list, const int *macro_str, Sym *args)
{
    int t, t0, t1, t2, n;
    const int *st;
    Sym *s;
    CValue cval;
    TokenString str;

#ifdef PP_DEBUG
    PP_PRINT(("asubst:", 0, macro_str));
    for (s = args, n = 0; s; s = s->prev, ++n);
    while (n--) {
        for (s = args, t = 0; t < n; s = s->prev, ++t);
        tok_print(s->d, "%*s - arg: %s:", indent, "", get_tok_str(s->v, 0));
    }
#endif

    tok_str_new(&str);
    t0 = t1 = 0;
    while(1) {
        TOK_GET(&t, &macro_str, &cval);
        if (!t)
            break;
        if (t == '#') {
            /* stringize */
            do t = *macro_str++; while (t == ' ');
            s = sym_find2(args, t);
            if (s) {
                cstr_reset(&tokcstr);
                cstr_ccat(&tokcstr, '\"');
                st = s->d;
                while (*st != TOK_EOF) {
                    const char *s;
                    TOK_GET(&t, &st, &cval);
                    s = get_tok_str(t, &cval);
                    while (*s) {
                        if (t == TOK_PPSTR && *s != '\'')
                            add_char(&tokcstr, *s);
                        else
                            cstr_ccat(&tokcstr, *s);
                        ++s;
                    }
                }
                cstr_ccat(&tokcstr, '\"');
                cstr_ccat(&tokcstr, '\0');
                //printf("\nstringize: <%s>\n", (char *)tokcstr.data);
                /* add string */
                cval.str.size = tokcstr.size;
                cval.str.data = tokcstr.data;
                tok_str_add2(&str, TOK_PPSTR, &cval);
#ifdef TCC_TARGET_ARM
            } else if ((parse_flags & PARSE_FLAG_ASM_FILE) && t == TOK_PPNUM) {
                /* for example: mov r1,#0 */
                --macro_str, tok_str_add(&str, '#');
#endif
            } else {
                expect("macro parameter after '#'");
            }
        } else if (t >= TOK_IDENT) {
            s = sym_find2(args, t);
            if (s) {
                st = s->d;
                n = 0;
                while ((t2 = macro_str[n]) == ' ')
                    ++n;
                /* if '##' is present before or after, no arg substitution */
                if (t2 == TOK_PPJOIN || t1 == TOK_PPJOIN) {
                    /* special case for var arg macros : ## eats the ','
                       if empty VA_ARGS variable. */
                    if (t1 == TOK_PPJOIN && t0 == ',' && gnu_ext && s->type.t) {
                        int c = str.str[str.len - 1];
                        while (str.str[--str.len] != ',')
                            ;
                        if (*st == TOK_EOF) {
                            /* suppress ',' '##' */
                        } else {
                            /* suppress '##' and add variable */
                            str.len++;
                            if (c == ' ')
                                str.str[str.len++] = c;
                            goto add_var;
                        }
                    } else {
                        if (*st == TOK_EOF)
                            tok_str_add(&str, TOK_PLCHLDR);
                    }
                } else {
            add_var:
		    if (!s->e) {
			/* Expand arguments tokens and store them.  In most
			   cases we could also re-expand each argument if
			   used multiple times, but not if the argument
			   contains the __COUNTER__ macro.  */
			TokenString str2;
			tok_str_new(&str2);
			macro_subst(&str2, nested_list, st);
			tok_str_add(&str2, TOK_EOF);
			s->e = str2.str;
		    }
		    st = s->e;
                }
                while (*st != TOK_EOF) {
                    TOK_GET(&t2, &st, &cval);
                    tok_str_add2(&str, t2, &cval);
                }
            } else {
                tok_str_add(&str, t);
            }
        } else {
            tok_str_add2(&str, t, &cval);
        }
        if (t != ' ')
            t0 = t1, t1 = t;
    }
    tok_str_add(&str, 0);
    PP_PRINT(("areslt:", 0, str.str));
    return str.str;
}

/* handle the '##' operator. return the resulting string (which must be freed). */
static inline int *macro_twosharps(const int *ptr0)
{
    int t1, t2, n, l;
    CValue cv1, cv2;
    TokenString macro_str1;
    const int *ptr;

    tok_str_new(&macro_str1);
    cstr_reset(&tokcstr);
    for (ptr = ptr0;;) {
        TOK_GET(&t1, &ptr, &cv1);
        if (t1 == 0)
            break;
        for (;;) {
            n = 0;
            while ((t2 = ptr[n]) == ' ')
                ++n;
            if (t2 != TOK_PPJOIN)
                break;
            ptr += n;
            while ((t2 = *++ptr) == ' ' || t2 == TOK_PPJOIN)
                ;
            TOK_GET(&t2, &ptr, &cv2);
            if (t2 == TOK_PLCHLDR)
                continue;
            if (t1 != TOK_PLCHLDR) {
                cstr_cat(&tokcstr, get_tok_str(t1, &cv1), -1);
                t1 = TOK_PLCHLDR;
            }
            cstr_cat(&tokcstr, get_tok_str(t2, &cv2), -1);
        }
        if (tokcstr.size) {
            cstr_ccat(&tokcstr, 0);
            tcc_open_bf(tcc_state, ":paste:", tokcstr.size);
            memcpy(file->buffer, tokcstr.data, tokcstr.size);
            tok_flags = 0; /* don't interpret '#' */
            for (n = 0;;n = l) {
                next_nomacro();
                tok_str_add2(&macro_str1, tok, &tokc);
                if (*file->buf_ptr == 0)
                    break;
                tok_str_add(&macro_str1, ' ');
                l = file->buf_ptr - file->buffer;
                tcc_warning("pasting \"%.*s\" and \"%s\" does not give a valid"
                    " preprocessing token", l - n, file->buffer + n, file->buf_ptr);
            }
            tcc_close();
            cstr_reset(&tokcstr);
        }
        if (t1 != TOK_PLCHLDR)
            tok_str_add2(&macro_str1, t1, &cv1);
    }
    tok_str_add(&macro_str1, 0);
    PP_PRINT(("pasted:", 0, macro_str1.str));
    return macro_str1.str;
}

static int peek_file (TokenString *ws_str)
{
    uint8_t *p = file->buf_ptr - 1;
    int c;
    for (;;) {
        PEEKC(c, p);
        switch (c) {
        case '/':
            PEEKC(c, p);
            if (c == '*')
                p = parse_comment(p);
            else if (c == '/')
                p = parse_line_comment(p);
            else {
                c = *--p = '/';
                goto leave;
            }
            --p, c = ' ';
            break;
        case ' ': case '\t':
            break;
        case '\f': case '\v': case '\r':
            continue;
        case '\n':
            file->line_num++, tok_flags |= TOK_FLAG_BOL;
            break;
        default: leave:
            file->buf_ptr = p;
            return c;
        }
        if (ws_str)
            tok_str_add(ws_str, c);
    }
}

/* peek or read [ws_str == NULL] next token from function macro call,
   walking up macro levels up to the file if necessary */
static int next_argstream(Sym **nested_list, TokenString *ws_str)
{
    int t;
    Sym *sa;

    while (macro_ptr) {
        const int *m = macro_ptr;
        while ((t = *m) != 0) {
            if (ws_str) {
                if (t != ' ')
                    return t;
                ++m;
            } else {
                TOK_GET(&tok, &macro_ptr, &tokc);
                return tok;
            }
        }
        end_macro();
        /* also, end of scope for nested defined symbol */
        sa = *nested_list;
        if (sa)
            *nested_list = sa->prev, sym_free(sa);
    }
    if (ws_str) {
        return peek_file(ws_str);
    } else {
        next_nomacro();
        if (tok == '\t' || tok == TOK_LINEFEED)
            tok = ' ';
        return tok;
    }
}

/* do macro substitution of current token with macro 's' and add
   result to (tok_str,tok_len). 'nested_list' is the list of all
   macros we got inside to avoid recursing. Return non zero if no
   substitution needs to be done */
static int macro_subst_tok(
    TokenString *tok_str,
    Sym **nested_list,
    Sym *s)
{
    int t;
    int v = s->v;

    PP_PRINT(("#", v, s->d));
    if (s->d) {
        int *mstr = s->d;
        int *jstr;
        Sym *sa;
        int ret;

        if (s->type.t & MACRO_FUNC) {
            int saved_parse_flags = parse_flags;
            TokenString str;
            int parlevel, i;
            Sym *sa1, *args;

            parse_flags |= PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED
                | PARSE_FLAG_ACCEPT_STRAYS;

            tok_str_new(&str);
            /* peek next token from argument stream */
            t = next_argstream(nested_list, &str);
            if (t != '(') {
                /* not a macro substitution after all, restore the
                 * macro token plus all whitespace we've read.
                 * whitespace is intentionally not merged to preserve
                 * newlines. */
                parse_flags = saved_parse_flags;
                tok_str_add2_spc(tok_str, v, 0);
                if (parse_flags & PARSE_FLAG_SPACES)
                    for (i = 0; i < str.len; i++)
                        tok_str_add(tok_str, str.str[i]);
                tok_str_free_str(str.str);
                return 0;
            } else {
                tok_str_free_str(str.str);
            }

            /* argument macro */
            args = NULL;
            sa = s->next;
            /* NOTE: empty args are allowed, except if no args */
            i = 2; /* eat '(' */
            for(;;) {
                do {
                    t = next_argstream(nested_list, NULL);
                } while (t == ' ' || --i);

                if (!sa) {
                    if (t == ')') /* handle '()' case */
                        break;
                    tcc_error("macro '%s' used with too many args",
                        get_tok_str(v, 0));
                }
            empty_arg:
                tok_str_new(&str);
                parlevel = 0;
                /* NOTE: non zero sa->type.t indicates VA_ARGS */
                while (parlevel > 0
                        || (t != ')' && (t != ',' || sa->type.t))) {
                    if (t == TOK_EOF)
                        tcc_error("EOF in invocation of macro '%s'",
                            get_tok_str(v, 0));
                    if (t == '(')
                        parlevel++;
                    if (t == ')')
                        parlevel--;
                    if (t == ' ')
                        str.need_spc |= 1;
                    else
                        tok_str_add2_spc(&str, t, &tokc);
                    t = next_argstream(nested_list, NULL);
                }
                tok_str_add(&str, TOK_EOF);
                sa1 = sym_push2(&args, sa->v & ~SYM_FIELD, sa->type.t, 0);
                sa1->d = str.str;
                sa = sa->next;
                if (t == ')') {
                    if (!sa)
                        break;
                    /* special case for gcc var args: add an empty
                       var arg argument if it is omitted */
                    if (sa->type.t && gnu_ext)
                        goto empty_arg;
                    tcc_error("macro '%s' used with too few args",
                        get_tok_str(v, 0));
                }
                i = 1;
            }

            /* now subst each arg */
            mstr = macro_arg_subst(nested_list, mstr, args);
            /* free memory */
            sa = args;
            while (sa) {
                sa1 = sa->prev;
                tok_str_free_str(sa->d);
                tok_str_free_str(sa->e);
                sym_free(sa);
                sa = sa1;
            }
            parse_flags = saved_parse_flags;
        }

        /* process '##'s (if present) */
        jstr = mstr;
        if (s->type.t & MACRO_JOIN)
            jstr = macro_twosharps(mstr);

        sa = sym_push2(nested_list, v, 0, 0);
        ret = macro_subst(tok_str, nested_list, jstr);
        /* pop nested defined symbol */
        if (sa == *nested_list)
            *nested_list = sa->prev, sym_free(sa);

        if (jstr != mstr)
            tok_str_free_str(jstr);
        if (mstr != s->d)
            tok_str_free_str(mstr);
        return ret;

    } else {
        CValue cval;
        char buf[32], *cstrval = buf;

        /* special macros */
        if (v == TOK___LINE__ || v == TOK___COUNTER__) {
            t = v == TOK___LINE__ ? file->line_num : pp_counter++;
            snprintf(buf, sizeof(buf), "%d", t);
            t = TOK_PPNUM;
            goto add_cstr1;

        } else if (v == TOK___FILE__) {
            cstrval = file->filename;
            goto add_cstr;

        } else if (v == TOK___DATE__ || v == TOK___TIME__) {
            time_t ti;
            struct tm *tm;
            time(&ti);
            tm = localtime(&ti);
            if (v == TOK___DATE__) {
                static char const ab_month_name[12][4] = {
                    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                };
                snprintf(buf, sizeof(buf), "%s %2d %d",
                    ab_month_name[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
            } else {
                snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                    tm->tm_hour, tm->tm_min, tm->tm_sec);
            }
        add_cstr:
            t = TOK_STR;
        add_cstr1:
            cval.str.size = strlen(cstrval) + 1;
            cval.str.data = cstrval;
            tok_str_add2_spc(tok_str, t, &cval);
        }
        return 0;
    }
}

/* do macro substitution of macro_str and add result to
   (tok_str,tok_len). 'nested_list' is the list of all macros we got
   inside to avoid recursing. */
static int macro_subst(
    TokenString *tok_str,
    Sym **nested_list,
    const int *macro_str
    )
{
    Sym *s;
    int t, nosubst = 0;
    CValue cval;
    TokenString *str;

#ifdef PP_DEBUG
    int tlen = tok_str->len;
    PP_PRINT(("+expand:", 0, macro_str));
#endif

    while (1) {
        TOK_GET(&t, &macro_str, &cval);
        if (t == 0 || t == TOK_EOF)
            break;
        if (t >= TOK_IDENT) {
            s = define_find(t);
            if (s == NULL || nosubst)
                goto no_subst;
            /* if nested substitution, do nothing */
            if (sym_find2(*nested_list, t)) {
                /* and mark so it doesn't get subst'd again */
                t |= SYM_FIELD;
                goto no_subst;
            }
            str = tok_str_alloc();
            str->str = (int*)macro_str; /* setup stream for possible arguments */
            begin_macro(str, 2);
            nosubst = macro_subst_tok(tok_str, nested_list, s);
            if (macro_stack != str) {
                /* already finished by reading function macro arguments */
                break;
            }
            macro_str = macro_ptr;
            end_macro ();
        } else if (t == ' ') {
            if (parse_flags & PARSE_FLAG_SPACES)
                tok_str->need_spc |= 1;
        } else {
    no_subst:
            tok_str_add2_spc(tok_str, t, &cval);
            if (nosubst && t != '(')
                nosubst = 0;
            /* GCC supports 'defined' as result of a macro substitution */
            if (t == TOK_DEFINED && pp_expr)
                nosubst = 1;
        }
    }

#ifdef PP_DEBUG
    tok_str_add(tok_str, 0), --tok_str->len;
    PP_PRINT(("-result:", 0, tok_str->str + tlen));
#endif
    return nosubst;
}

/* return next token with macro substitution */
ST_FUNC void next(void)
{
    int t;
    while (macro_ptr) {
redo:
        t = *macro_ptr;
        if (TOK_HAS_VALUE(t)) {
            tok_get(&tok, &macro_ptr, &tokc);
            if (t == TOK_LINENUM) {
                file->line_num = tokc.i;
                goto redo;
            }
            goto convert;
        } else if (t == 0) {
            /* end of macro or unget token string */
            end_macro();
            continue;
        } else if (t == TOK_EOF) {
            /* do nothing */
        } else {
            ++macro_ptr;
            t &= ~SYM_FIELD; /* remove 'nosubst' marker */
            if (t == '\\') {
                if (!(parse_flags & PARSE_FLAG_ACCEPT_STRAYS))
                    tcc_error("stray '\\' in program");
            }
        }
        tok = t;
        return;
    }

    next_nomacro();
    t = tok;
    if (t >= TOK_IDENT && (parse_flags & PARSE_FLAG_PREPROCESS)) {
        /* if reading from file, try to substitute macros */
        Sym *s = define_find(t);
        if (s) {
            Sym *nested_list = NULL;
            macro_subst_tok(&tokstr_buf, &nested_list, s);
            tok_str_add(&tokstr_buf, 0);
            begin_macro(&tokstr_buf, 0);
            goto redo;
        }
        return;
    }

convert:
    /* convert preprocessor tokens into C tokens */
    if (t == TOK_PPNUM) {
        if  (parse_flags & PARSE_FLAG_TOK_NUM)
            parse_number(tokc.str.data);
    } else if (t == TOK_PPSTR) {
        if (parse_flags & PARSE_FLAG_TOK_STR)
            parse_string(tokc.str.data, tokc.str.size - 1);
    }
}

/* push back current token and set current token to 'last_tok'. Only
   identifier case handled for labels. */
ST_INLN void unget_tok(int last_tok)
{
    TokenString *str = &unget_buf;
    int alloc = 0;
    if (str->len) /* use static buffer except if already in use */
        str = tok_str_alloc(), alloc = 1;
    if (tok != TOK_EOF)
        tok_str_add2(str, tok, &tokc);
    tok_str_add(str, 0);
    begin_macro(str, alloc);
    tok = last_tok;
}

/* ------------------------------------------------------------------------- */
/* init preprocessor */

static const char * const target_os_defs =
#ifdef TCC_TARGET_PE
    "_WIN32\0"
# if PTR_SIZE == 8
    "_WIN64\0"
# endif
#else
# if defined TCC_TARGET_MACHO
    "__APPLE__\0"
# elif TARGETOS_FreeBSD
    "__FreeBSD__ 12\0"
# elif TARGETOS_FreeBSD_kernel
    "__FreeBSD_kernel__\0"
# elif TARGETOS_NetBSD
    "__NetBSD__\0"
# elif TARGETOS_OpenBSD
    "__OpenBSD__\0"
# else
    "__linux__\0"
    "__linux\0"
#  if TARGETOS_ANDROID
    "__ANDROID__\0"
#  endif
# endif
    "__unix__\0"
    "__unix\0"
#endif
    ;

static void putdef(CString *cs, const char *p)
{
    cstr_printf(cs, "#define %s%s\n", p, &" 1"[!!strchr(p, ' ')*2]);
}

static void putdefs(CString *cs, const char *p)
{
    while (*p)
        putdef(cs, p), p = strchr(p, 0) + 1;
}

static void tcc_predefs(TCCState *s1, CString *cs, int is_asm)
{
    cstr_printf(cs, "#define __TINYC__ 9%.2s\n", &TCC_VERSION[4]);
    putdefs(cs, target_machine_defs);
    putdefs(cs, target_os_defs);

#ifdef TCC_TARGET_ARM
    if (s1->float_abi == ARM_HARD_FLOAT)
      putdef(cs, "__ARM_PCS_VFP");
#endif
    if (is_asm)
      putdef(cs, "__ASSEMBLER__");
    if (s1->output_type == TCC_OUTPUT_PREPROCESS)
      putdef(cs, "__TCC_PP__");
    if (s1->output_type == TCC_OUTPUT_DESUGAR) {
      putdef(cs, "__TCC_PP__");   /* 脱糖产物是预处理后的标准C */
      putdef(cs, "__TCC_DESUGAR__");
    }
    if (s1->output_type == TCC_OUTPUT_MEMORY)
      putdef(cs, "__TCC_RUN__");
#ifdef CONFIG_TCC_BACKTRACE
    if (s1->do_backtrace)
      putdef(cs, "__TCC_BACKTRACE__");
#endif
#ifdef CONFIG_TCC_BCHECK
    if (s1->do_bounds_check)
      putdef(cs, "__TCC_BCHECK__");
#endif
    if (s1->char_is_unsigned)
      putdef(cs, "__CHAR_UNSIGNED__");
    if (s1->optimize > 0)
      putdef(cs, "__OPTIMIZE__");
    if (s1->option_pthread)
      putdef(cs, "_REENTRANT");
    if (s1->leading_underscore)
      putdef(cs, "__leading_underscore");
    cstr_printf(cs, "#define __SIZEOF_POINTER__ %d\n", PTR_SIZE);
    cstr_printf(cs, "#define __SIZEOF_LONG__ %d\n", LONG_SIZE);
    if (!is_asm) {
      putdef(cs, "__STDC__");
      cstr_printf(cs, "#define __STDC_HOSTED__ %d\n", s1->nostdlib ? 0 : 1);
      cstr_printf(cs, "#define __STDC_VERSION__ %dL\n", s1->cversion);
      cstr_cat(cs,
        /* load more predefs and __builtins */
#if CONFIG_TCC_PREDEFS
        #include "tccdefs_.h" /* include as strings */
#else
        "#include <tccdefs.h>\n" /* load at runtime */
#endif
        , -1);
    }
    cstr_printf(cs, "#define __BASE_FILE__ \"%s\"\n", file->filename);
}

ST_FUNC void preprocess_start(TCCState *s1, int filetype)
{
    int is_asm = !!(filetype & (AFF_TYPE_ASM|AFF_TYPE_ASMPP));

    tccpp_new(s1);

    s1->include_stack_ptr = s1->include_stack;
    s1->ifdef_stack_ptr = s1->ifdef_stack;
    file->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    pp_expr = 0;
    pp_counter = 0;
    pp_debug_tok = pp_debug_symv = 0;
    s1->pack_stack[0] = 0;
    s1->pack_stack_ptr = s1->pack_stack;

    set_idnum('$', s1->dollars_in_identifiers ? IS_ID : 0);
    set_idnum('.', is_asm ? IS_ID : 0);

    if (!(filetype & AFF_TYPE_ASM)) {
        CString cstr;
        cstr_new(&cstr);
        tcc_predefs(s1, &cstr, is_asm);
        if (s1->cmdline_defs.size)
          cstr_cat(&cstr, s1->cmdline_defs.data, s1->cmdline_defs.size);
        if (s1->cmdline_incl.size)
          cstr_cat(&cstr, s1->cmdline_incl.data, s1->cmdline_incl.size);
        //printf("%.*s\n", cstr.size, (char*)cstr.data);
        *s1->include_stack_ptr++ = file;
        tcc_open_bf(s1, "<command line>", cstr.size);
        memcpy(file->buffer, cstr.data, cstr.size);
        cstr_free(&cstr);
    }
    parse_flags = is_asm ? PARSE_FLAG_ASM_FILE : 0;
}

/* cleanup from error/setjmp */
ST_FUNC void preprocess_end(TCCState *s1)
{
    while (macro_stack)
        end_macro();
    macro_ptr = NULL;
    while (file)
        tcc_close();
    tccpp_delete(s1);
}

ST_FUNC int set_idnum(int c, int val)
{
    int prev = isidnum_table[c - CH_EOF];
    isidnum_table[c - CH_EOF] = val;
    return prev;
}

ST_FUNC void tccpp_new(TCCState *s)
{
    int i, c;
    const char *p, *r;

    /* init isid table */
    for(i = CH_EOF; i<128; i++)
        set_idnum(i,
            is_space(i) ? IS_SPC
            : isid(i) ? IS_ID
            : isnum(i) ? IS_NUM
            : 0);

    for(i = 128; i<256; i++)
        set_idnum(i, IS_ID);

    /* init allocators */
    tal_new(&toksym_alloc, TOKSYM_TAL_SIZE);
    tal_new(&tokstr_alloc, TOKSTR_TAL_SIZE);

    memset(hash_ident, 0, TOK_HASH_SIZE * sizeof(TokenSym *));
    memset(s->cached_includes_hash, 0, sizeof s->cached_includes_hash);

    cstr_new(&tokcstr);
    cstr_new(&cstr_buf);
    cstr_realloc(&cstr_buf, STRING_MAX_SIZE);
    tok_str_new(&unget_buf);
    tok_str_realloc(&unget_buf, TOKSTR_MAX_SIZE);
    tok_str_new(&tokstr_buf);
    tok_str_realloc(&tokstr_buf, TOKSTR_MAX_SIZE);

    tok_ident = TOK_IDENT;
    p = tcc_keywords;
    while (*p) {
        r = p;
        for(;;) {
            c = *r++;
            if (c == '\0')
                break;
        }
        tok_alloc(p, r - p - 1);
        p = r;
    }

    /* we add dummy defines for some special macros to speed up tests
       and to have working defined() */
    define_push(TOK___LINE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___FILE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___DATE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___TIME__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___COUNTER__, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void tccpp_delete(TCCState *s)
{
    int i, n;

    dynarray_reset(&s->cached_includes, &s->nb_cached_includes);

    /* free tokens */
    n = tok_ident - TOK_IDENT;
    if (n > total_idents)
        total_idents = n;
    for (i = n; --i >= 0;)
        tal_free(&toksym_alloc, table_ident[i]);
    tcc_free(table_ident);
    table_ident = NULL;

    /* free static buffers */
    cstr_free(&tokcstr);
    cstr_free(&cstr_buf);
    tok_str_free_str(tokstr_buf.str);
    tok_str_free_str(unget_buf.str);

    /* free allocators */
    tal_delete(&toksym_alloc);
    tal_delete(&tokstr_alloc);
}

/* ------------------------------------------------------------------------- */
/* tcc -E [-P[1]] [-dD} support */

static int pp_need_space(int a, int b);

static void tok_print(const int *str, const char *msg, ...)
{
    FILE *fp = tcc_state->ppfp;
    va_list ap;
    int t, t0, s;
    CValue cval;

    va_start(ap, msg);
    vfprintf(fp, msg, ap);
    va_end(ap);

    s = t0 = 0;
    while (str) {
	TOK_GET(&t, &str, &cval);
	if (t == 0 || t == TOK_EOF)
	    break;
        if (pp_need_space(t0, t))
            s = 0;
	fprintf(fp, &" %s"[s], t == TOK_PLCHLDR ? "<>" : get_tok_str(t, &cval));
        s = 1, t0 = t;
    }
    fprintf(fp, "\n");
}

static void pp_line(TCCState *s1, BufferedFile *f, int level)
{
    int d = f->line_num - f->line_ref;

    if (s1->dflag & 4)
	return;

    if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_NONE) {
        ;
    } else if (level == 0 && f->line_ref && d < 8) {
	while (d > 0)
	    fputs("\n", s1->ppfp), --d;
    } else if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_STD) {
	fprintf(s1->ppfp, "#line %d \"%s\"\n", f->line_num, f->filename);
    } else {
	fprintf(s1->ppfp, "# %d \"%s\"%s\n", f->line_num, f->filename,
	    level > 0 ? " 1" : level < 0 ? " 2" : "");
    }
    f->line_ref = f->line_num;
}

static void define_print(TCCState *s1, int v)
{
    FILE *fp;
    Sym *s;

    s = define_find(v);
    if (NULL == s || NULL == s->d)
        return;

    fp = s1->ppfp;
    fprintf(fp, "#define %s", get_tok_str(v, NULL));
    if (s->type.t & MACRO_FUNC) {
        Sym *a = s->next;
        fprintf(fp,"(");
        if (a)
            for (;;) {
                fprintf(fp,"%s", get_tok_str(a->v, NULL));
                if (!(a = a->next))
                    break;
                fprintf(fp,",");
            }
        fprintf(fp,")");
    }
    tok_print(s->d, "");
}

static void pp_debug_defines(TCCState *s1)
{
    int v, t;
    const char *vs;
    FILE *fp;

    t = pp_debug_tok;
    if (t == 0)
        return;

    file->line_num--;
    pp_line(s1, file, 0);
    file->line_ref = ++file->line_num;

    fp = s1->ppfp;
    v = pp_debug_symv;
    vs = get_tok_str(v, NULL);
    if (t == TOK_DEFINE) {
        define_print(s1, v);
    } else if (t == TOK_UNDEF) {
        fprintf(fp, "#undef %s\n", vs);
    } else if (t == TOK_push_macro) {
        fprintf(fp, "#pragma push_macro(\"%s\")\n", vs);
    } else if (t == TOK_pop_macro) {
        fprintf(fp, "#pragma pop_macro(\"%s\")\n", vs);
    }
    pp_debug_tok = 0;
}

/* Add a space between tokens a and b to avoid unwanted textual pasting */
static int pp_need_space(int a, int b)
{
    return 'E' == a ? '+' == b || '-' == b
        : '+' == a ? TOK_INC == b || '+' == b
        : '-' == a ? TOK_DEC == b || '-' == b
        : a >= TOK_IDENT || a == TOK_PPNUM ? b >= TOK_IDENT || b == TOK_PPNUM
        : 0;
}

/* maybe hex like 0x1e */
static int pp_check_he0xE(int t, const char *p)
{
    if (t == TOK_PPNUM && toup(strchr(p, 0)[-1]) == 'E')
        return 'E';
    return t;
}

/* --------------------------------------------------------------------- */
/* operator 脱糖 (token 层): 把 struct 二元算术 (operator<op> 重载) 降级为
 * 标准 C 函数调用。
 *
 * 词法事实: 预处理层把 `operator` 切为独立关键字 token (TOK_OPERATOR),
 * 其后的运算符字符 `+` 是独立 token。故本模块以"逻辑算符"看待
 * `operator` + 紧随运算符两 token, 而非单一 `operator+` 字符串。
 *
 * 1. 定义处: `struct Vec3 operator+ (...)` 中的 `operator`+`+` 改名为
 *    合法标准 C 名 `operator_add`, 并收集:
 *       dg_op_reg[opchar] -> op id   (注册了哪些二元运算符)
 *       dg_op_tag[opid]   -> struct tag token (operator 的返回/操作数类型)
 *    只有注册过 operator 的编译才会启用改写 (dg_active).
 * 2. 调用处: 对赋值右值做完全括号的忠实改写 (优先级/结合性无损):
 *       a + b*b  ->  operator_add(a, operator_mul(b, b))
 *    仅当 RHS 是"简单算术" (标识符/字面量/括号及其四则算符), 且左右任一侧是
 *    operator 类型变量 (dg_var 命中) 才展开; 含函数调用/下标/字符串等
 *    一律原样透传, 普通 `int x+y` 也不受影响 -> 无误伤.
 * 3. 零回归: 未出现 operator 定义的编译 (t052/simd_demo/t046) 全程不启用
 *    (dg_active==0), token 经文本快照原样透传, 与 --emit-c 现有行为一致.
 *
 * 重要: token 的 CValue (数字/字符串) 在 next() 后失效, 故缓冲时即对
 * 每个 token 的文本做快照 (dg_txt), 重放/flush 全部基于快照, 避免脏读.
 */

/* ---- 可重载运算符表 (token -> 脱糖标准C名后缀, 返回类别) ----
 * kind: 'B' 二元算术(返回 struct tag), 'C' 比较(返回 int), 'U' 一元(返回 tag),
 *       'I' 自增自减(返回 tag; TCC -run 前后缀统一存回操作数且结果=新值, 脱糖同效). */
#define DG_OPN 5     /* 二元算术: + - * / % */
#define DG_OPU 2     /* 一元: ! ~ */
#define DG_OPI 2     /* 自增自减: ++ -- */
#define DG_OPC 6     /* 比较: == != < <= > >= */
#define DG_MAXOP (DG_OPN + DG_OPU + DG_OPI + DG_OPC)
typedef struct { int tok; const char *wrd; char kind; } DGOP_t;
static const DGOP_t dg_op_tbl[DG_MAXOP] = {
    { '+', "add", 'B' }, { '-', "sub", 'B' }, { '*', "mul", 'B' },
    { '/', "div", 'B' }, { '%', "mod", 'B' },
    { '!', "bang", 'U' }, { '~', "til", 'U' },
    { TOK_INC, "inc", 'I' }, { TOK_DEC, "dec", 'I' },
    { TOK_EQ, "eq", 'C' }, { TOK_NE, "ne", 'C' }, { TOK_LT, "lt", 'C' },
    { TOK_LE, "le", 'C' }, { TOK_GT, "gt", 'C' }, { TOK_GE, "ge", 'C' },
};
static char dg_op_name[DG_MAXOP + 1][24];  /* [op id] = "operator_add" ... */
static char dg_op_kind[DG_MAXOP + 1];      /* [op id] = 'B'/'C'/'U'/'I' */
static unsigned char dg_oreg[DG_MAXOP + 1];/* [op id] = 该运算符已被 operator 定义注册 (1/0) */
static int dg_op_tag[DG_MAXOP + 1];        /* [op id] = struct tag token, 0 无 */
static int dg_active;                      /* 出现过 operator 定义 -> 启用改写 */

/* operator 脱糖定义/调用名: `operator_<wrd>[_<typeBase>]`(typeBase 空则无后缀,
 * 兼容旧名 operator_lt). 使分派不依赖定义用何种运算符符号, 且同名 operator
 * 用于不同类型时生成 operator_lt_Cmp / operator_lt_Other 互不冲突. */
static const char *dg_op_nm_txt(int opid, const char *typ)
{
    static char nm[88];
    size_t l;
    snprintf(nm, sizeof nm, "%s", dg_op_name[opid]);
    if (typ && *typ) {
        l = strlen(nm);
        if (l + 2 + strlen(typ) < sizeof nm) {
            strcat(nm, "_");
            strcat(nm, typ);
        }
    }
    return nm;
}

/* 运算符 token 的文本形式 (算术单字符, 比较/自增减多字符) */
static const char *dg_optxt(int t)
{
    static char b[3];
    switch (t) {
    case '+': return "+";  case '-': return "-";  case '*': return "*";
    case '/': return "/";  case '%': return "%";  case '!': return "!";
    case '~': return "~";
    case TOK_EQ: return "==";  case TOK_NE: return "!=";
    case TOK_LT: return "<";   case TOK_LE: return "<=";
    case TOK_GT: return ">";   case TOK_GE: return ">=";
    case TOK_INC: return "++"; case TOK_DEC: return "--";
    default: b[0] = (char)t; b[1] = 0; return b;
    }
}
/* 比较类 token? */
static int dg_iscmp(int t)
{
    return t == TOK_EQ || t == TOK_NE || t == TOK_LT
        || t == TOK_LE || t == TOK_GT || t == TOK_GE;
}

/* token -> op id (1..DG_MAXOP), 无关是否已注册. 覆盖单字符算术(+ - * / % ! ~)
 * 与多字符 token (== != < <= > >= ++ --). */
static int dg_opid(int tok)
{
    int i;
    for (i = 0; i < DG_MAXOP; i++)
        if (dg_op_tbl[i].tok == tok)
            return i + 1;
    return 0;
}

/* 该 token 对应的运算符是否已注册 (op id 有效且已被 operator 定义注册) */
static int dg_oreg_tok(int tok)
{
    int id = dg_opid(tok);
    return id && dg_oreg[id];
}

static int dg_var_nm[8192];       /* var token */
static int dg_var_tag[8192];      /* var 对应的 struct tag token */
static int dg_var_ptr[8192];      /* var 是否为 struct 指针 (声明为 A* / 显式 &) */
static int dg_varcnt;

/* 语句级 token 缓冲 (收集到 LINEFEED 再整体处理) */
#define DG_BUFN 4096
static int dg_buf[DG_BUFN];       /* token 值 (判 is_space / TOK_OPERATOR / '=') */
static char *dg_txt[DG_BUFN];     /* 文本快照 (缓冲时拷贝, 重放稳定) */
static int dg_n;
/* 每行配对索引: 每个 flush 构建一次, 供 if/while 条件、defer 分号扫描、赋值
 * '[]' 深度等所有改写 pass 读取 —— 取代各处各自手写的括号计数.
 * dg_pair[i] = 与 i 配对的括号下标 (i 为 ()[]{}) 之一, 无配对为 -1. */
static int dg_pair[DG_BUFN];

/* operator 表达式节点 (用于完全括号忠实改写).
 * op: 0=叶; 正整数=二元运算符字符; 负值为扩展节点:
 *   DG_OP_TERN  三元 cond?then:else  (l=cond, r=then, x=else)
 *   DG_OP_CALL  函数调用           (l=被调函数叶, r=实参逗号链)
 *   DG_OP_CM    逗号链 (实参)      (l=实参节点, r=下一个 CM 节点 或 -1) */
#define DG_NODEN 8192
#define DG_OP_TERN (-1)
#define DG_OP_CALL (-2)
#define DG_OP_CM   (-3)
typedef struct {
    int op;                  /* 0=叶, 正整数=二元运算符字符 token, 负值为扩展节点 */
    int l, r;                /* 子节点索引 (叶为 -1) */
    int x;                   /* 第三个节点 (三元 else) */
    int tag;                 /* 叶若为 operator 类型变量则为 tag token */
    char txt[64];            /* 叶文本 */
} DGNode;
static DGNode dg_ndo[DG_NODEN];
static int dg_nndo;

/* defer 脱糖: 按作用域收集 `func(args);` 调用文本, 闭块处逆序重放.
 * TCC 的 defer 是"注册点求值参数、离开作用域逆序调用"; 脱糖在闭块点逆序发射
 * 调用文本。仅覆盖块正常结束的落地路径 (return/goto/break 提前退出未展开,
 * 仍以 TCC -run 为准)。 */
#define DG_DEFER_MAXDEP 32
#define DG_DEFER_MAXN  128
static char *dg_defer[DG_DEFER_MAXDEP][DG_DEFER_MAXN];
static int dg_defer_n[DG_DEFER_MAXDEP];
static int dg_dep;                 /* 当前**语句块**深度 (函数体内 >= 1); 初始化器/聚合
                                    花括号不计入, 由 dg_ini 单独跟踪 (跨行存活) */
static int dg_ini;                 /* 当前未闭合的初始化器/聚合花括号深度 */
static int dg_refln;               /* 已登记的 reflect 类型数 (前向声明, 定义在 reflect 小节) */
static int dg_used_reflect;        /* 是否出现 __builtin_reflect (仅其存在才 emit 反射表) */

/* 语句分类结果: 一次前向扫描产出语句形状与关键 span, dg_flush 依它单一分发.
 * 取代原先散落在 dg_flush 里的 括号深度/return/if-while/复合赋值/自增减/顶层=
 * 多套状态机 pass; 分类扫描同步维护 defer 块深度 (闭块层依 token 序记入 cls). */
#define DG_STMT_OTHER   0
#define DG_STMT_IFWHILE 1
#define DG_STMT_CA      2   /* 复合赋值 a op= b (op ∈ + - * / %) */
#define DG_STMT_INCDEC  3   /* 自增自减 ++a / a++ / --a / a-- */
#define DG_STMT_RETURN  4   /* return <expr>; */
#define DG_STMT_ASSIGN  5   /* 顶层 a = <expr>; */
typedef struct {
    int kind;              /* DG_STMT_* (调试/可读; 分发仍按 *_ok 标志链) */
    int first;             /* 首个非空格 token 下标; -1 = 空行 */
    int ifw;               /* 1 = 本行以 if/while 开头 */
    int bopn, bclo;        /* IFWHILE: 条件开括号与配对闭括号 */
    int ca, cabase, calh, casem, ca_ok; /* 复合赋值: op 位置/基础算子/LHS/';' */
    int inc, incspec, incsem, inc_ok;   /* 自增减: op 位置/操作数/';' */
    int eq;                /* 首个顶层 '=' (仅 [] 计入深度, 与旧行为一致) */
    int ret;               /* 首个 return */
    int isret;             /* 本行以 return 开头 (早退 defer 落地) */
    int isexit;            /* 本行以 goto/break/continue 开头 (跳出作用域早退) */
    int ncls;              /* 本行闭合的语句块层数 */
    int cls[DG_DEFER_MAXDEP]; /* 依 token 序的闭合块层 (defer 逆序重放) */
} DgStmt;

static int dg_expr(int *pi, int min_prec);
static int dg_mkbin(int optok, int l, int r);
static int dg_mkun(int optok, int l);
static int dg_mktern(int c, int t, int e);

/* model 泛型状态: 前向声明 (完整定义在 model 小节), 供 dg_reset 清理 */
typedef struct DgModelDef DgModelDef;
typedef struct { int t; char *txt; } DgTk;   /* model 定义 token: 值 + 文本快照(owned) */
static DgTk *dgm;         /* 跨行累积的 model 定义 token 序列(增长式, 替换文本缓冲 dg_mb) */
static int dgm_n, dgm_cap;
static void dgm_add(int t, const char *txt);   /* 前向: 定义/收集在 reset 之后 */
static void dgm_free(void);
static const char *dgm_str(int i);
static int dgm_is(int i, const char *word, int ch);
static DgModelDef *dg_model_def;
static int dg_mc;
static int dg_mbr;
static int dg_msemi;
static int dg_mbody;    /* 已进入定义体(首 token 后置1), 支撑函数泛型以 } 收尾 */
static int dg_mbase;    /* 定义体开括号处的 dg_mbr (顶层深度基准); -1=尚未见体开 "{" */
static char *dg_model_out[512];
static int dg_model_nout;
static char *dg_fout_type[512];   /* 函数定义内累积的文件作用域 struct typedef 名去重 */
static int dg_fout_typed;

/* 函数泛型实例: 合成定义体延迟到文件末尾(函数作用域)发射.
 * 原型在实例化点(语句级, `static ret name(args);` 合法)发射; 定义在 EOF 处
 * 以文件作用域落地, 使 `Name(targs)(...)` 调用改写为可链接的合成函数名. */
typedef struct DgModelFDef {
    char *synth;                 /* 合成函数名 Name_Arg...(同 dg_model_synth) */
    char *proto;                 /* 语句级原型文本 */
    char *def;                   /* 文件作用域定义文本 (含所需嵌套 struct typedef) */
    int  proto_printed;
    struct DgModelFDef *next;
} DgModelFDef;
static DgModelFDef *dg_fdefs;
static CString dg_fout_td;        /* 函数泛型定义所需的文件作用域 struct typedef 累积 */
static int  dg_fout_td_init;
static FILE *dg_tmpfile;          /* --emit-c 函数泛型: 主体缓冲临时文件 */
static FILE *dg_saved_ppfp;       /* --emit-c 函数泛型: 真实的输出流 (EOF 回放) */
static int  dg_buffering;
/* 正被展开定义的函数泛型名与其合成实例名: 支撑泛型体**自递归**绑定 ——
 * body 里裸引用 `<func>(args)`(无类型实参)改写为 `<synth>(args)`. */
static const char *dg_fdef_name;
static char dg_fdef_synth[384];
static void dg_model_reset_impl(void); /* 结构体完整后再定义 */

static void dg_reset(void)
{
    int i, d;
    for (i = 1; i <= DG_MAXOP; i++) {
        dg_oreg[i] = 0;
        dg_op_tag[i] = 0;
        dg_op_kind[i] = dg_op_tbl[i - 1].kind;
        strcpy(dg_op_name[i], "operator_");
        strcat(dg_op_name[i], dg_op_tbl[i - 1].wrd);
    }
    dg_oreg[0] = 0;
    dg_active = 0;
    dg_varcnt = 0;
    dg_n = 0;
    /* model 泛型: 清空模板表 / 已发射 typedef 名 / 收集态 */
    dg_model_reset_impl();
    /* defer 栈: 清空各深度计数, 释放残留文本 (fresh start 通常为空) */
    for (d = 0; d < DG_DEFER_MAXDEP; d++) {
        for (i = 0; i < dg_defer_n[d]; i++)
            if (dg_defer[d][i]) {
                tcc_free(dg_defer[d][i]);
                dg_defer[d][i] = NULL;
            }
        dg_defer_n[d] = 0;
    }
    dg_dep = 0;
    dg_ini = 0;
    dg_refln = 0;
    dg_used_reflect = 0;
}

/* op token -> registered op id (1..DG_MAXOP), 0 = 未注册/无此运算符 */
static int dg_opatid(int tok)
{
    int id = dg_opid(tok);
    return id && dg_oreg[id] ? id : 0;
}

/* 从 i+1 起的下一个非空格 token 下标; 无则 -1 */
static int dg_next_ns(int i)
{
    int j;
    for (j = i + 1; j < dg_n; j++)
        if (!is_space(dg_buf[j]))
            return j;
    return -1;
}

/* dg_buf[i] 若为 TOK_OPERATOR 且其后紧跟已识别运算符 (单字符算术 / 多字符
 * 比较 / 自增减 token), 返回该 opid; 否则 0. 顺带把该运算符注册 (标记已定义). */
static int dg_opat(int i)
{
    int j, opid;
    if (dg_buf[i] != TOK_OPERATOR)
        return 0;
    j = dg_next_ns(i);
    if (j < 0)
        return 0;
    opid = dg_opid(dg_buf[j]);
    if (opid)
        dg_oreg[opid] = 1;
    return opid;
}

/* dg_buf[i] 为 `operator_<wrd>` 标识符 (比较运算符常用写法: operator_eq / operator_ne
 * / operator_lt ...; tcc 按最长匹配将其把整个标识符一次性 tokenize, 而非 TOK_OPERATOR
 * 关键字). 命中词表返回相应 op id 并注册; 否则 0. */
static int dg_opat_word(int i)
{
    const char *nm;
    int k;
    if (dg_buf[i] < TOK_IDENT)
        return 0;
    nm = dg_txt[i] ? dg_txt[i] : "";
    if (strncmp(nm, "operator_", 9) != 0)
        return 0;
    for (k = 0; k < DG_MAXOP; k++)
        if (strcmp(nm + 9, dg_op_tbl[k].wrd) == 0) {
            int id = k + 1;
            dg_oreg[id] = 1;
            return id;
        }
    return 0;
}

/* 变量 token -> struct tag; 返回 0 表示非 operator 类型变量 */
static int dg_var_of(int tok)
{
    int i;
    for (i = 0; i < dg_varcnt; i++)
        if (dg_var_nm[i] == tok)
            return dg_var_tag[i];
    return 0;
}

static int dg_var_isptr(int tok)
{
    int i;
    for (i = 0; i < dg_varcnt; i++)
        if (dg_var_nm[i] == tok)
            return dg_var_ptr[i];
    return 0;
}

static int dg_add_var(int tok, int tag)
{
    if (tok <= 0 || tag <= 0)
        return 0;
    if (dg_var_of(tok))
        return 0;
    if (dg_varcnt < 8192) {
        dg_var_nm[dg_varcnt] = tok;
        dg_var_tag[dg_varcnt] = tag;
        dg_var_ptr[dg_varcnt] = 0;
        dg_varcnt++;
    }
    return 1;
}

/* 登记变量时为已登记的 var 更新指针性(dg_add_var 后), 用于方法糖 recv 值/指针判定 */
static void dg_var_setptr(int tok)
{
    int i;
    for (i = 0; i < dg_varcnt; i++)
        if (dg_var_nm[i] == tok)
            dg_var_ptr[i] = 1;
}

/* 按值覆写 var 的指针性("最近声明优先"; 处理函数参数与局部同名变量),
 * 未登记则不新增。val: 0=值 1=指针 */
static void dg_var_setptr2(int tok, int val)
{
    int i;
    for (i = 0; i < dg_varcnt; i++)
        if (dg_var_nm[i] == tok)
            dg_var_ptr[i] = val;
}

/* ---- 完全括号/忠实改写: 右值 -> DGNode 树 ---- */
static int dg_tokchar(int i)
{
    return dg_txt[i] ? (unsigned char)dg_txt[i][0] : 0;
}

/* 运算符 token 的二元优先级. 单字符算术 / 多字符比较; 非二元返回 0.
 * 关系/比较也纳入 (宽松绑定), 命中 operator_* 时改写, 否则原样透传. */
static int dg_prec(int tok)
{
    switch (tok) {
    case '*': case '/': case '%': return 3;
    case '+': case '-': return 2;
    case TOK_LT: case TOK_LE: case TOK_GT: case TOK_GE:
    case TOK_EQ: case TOK_NE: return 1;   /* 比较: 宽松绑定 */
    default: return 0;
    }
}

static int dg_tok_simple(int i)
{
    int ch;
    if (is_space(dg_buf[i]))
        return 1;
    if (dg_buf[i] == TOK_OPERATOR)
        return 0;
    if (dg_buf[i] == TOK_INC || dg_buf[i] == TOK_DEC)
        return 1;                                   /* ++/-- cursor */
    ch = dg_tokchar(i);
    if (isid(ch) || isnum(ch) || ch == '_' || ch == '.')
        return 1;
    if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%'
        || ch == '(' || ch == ')' || ch == ',' || ch == '?' || ch == ':'
        || ch == '<' || ch == '>' || ch == '=' || ch == '!' || ch == '~')
        return 1;
    /* 复合赋值 += 等 (多字符 token 首字符为运算符) */
    return TOK_ASSIGN(dg_buf[i]);
}

/* 右值片段 [from,to) 是否可进入改写 (放宽: 真正安全由 parse-boundary +
 * has_rewrite 双重把关, 此处仅拦截毫不相关/悬空的片段) */
static int dg_check_simple(int from, int to)
{
    int i;
    for (i = from; i < to; i++)
        if (!dg_tok_simple(i))
            return 0;
    return 1;
}

static int dg_newleaf(int i)
{
    DGNode *nd;
    if (dg_nndo >= DG_NODEN)
        return -1;
    nd = &dg_ndo[dg_nndo];
    nd->op = 0; nd->l = nd->r = -1;
    nd->tag = dg_var_of(dg_buf[i]);
    snprintf(nd->txt, sizeof nd->txt, "%s", dg_txt[i] ? dg_txt[i] : "");
    return dg_nndo++;
}

/* 节点是否为 operator 类型 (叶: 已登记 struct 变量; 二元: 两侧 op 且 op 已注册, 类型向上传播) */
static int dg_optagged(int n)
{
    if (n < 0)
        return 0;
    return dg_ndo[n].tag != 0;
}
/* 兼容旧名: 判断节点是否 operator 类型运算数 */
static int dg_simplevar_node(int n)
{
    return dg_optagged(n);
}

/* 树中是否含"命中 operator 的展开": 需 op 已注册且操作数为 operator 类型
 *  ('B'/'C' 二元: 两侧均需; 'U' 一元/自增减: 单侧即可). */
static int dg_has_rewrite(int n)
{
    DGNode *nd;
    int id;
    if (n < 0)
        return 0;
    nd = &dg_ndo[n];
    if (nd->op == 0)
        return 0;
    if (nd->op == DG_OP_CALL)
        return dg_has_rewrite(nd->r);   /* 被调函数是叶(tag0), 只需查实参链 */
    if (nd->op == DG_OP_CM)
        return dg_has_rewrite(nd->l) || dg_has_rewrite(nd->r);
    if (nd->op == DG_OP_TERN)
        return dg_has_rewrite(nd->l) || dg_has_rewrite(nd->r) || dg_has_rewrite(nd->x);
    id = dg_opid(nd->op);
    if (id && dg_oreg[id]) {
        if (dg_op_kind[id] == 'U' || dg_op_kind[id] == 'I')
            return dg_optagged(nd->l) || dg_has_rewrite(nd->l);
        if (dg_optagged(nd->l) && dg_optagged(nd->r))
            return 1;               /* 二元('.')与比较('C'): 两侧命中即改写 */
    }
    return dg_has_rewrite(nd->l) || dg_has_rewrite(nd->r);
}

/* 构建函数调用节点: callee 为标识符叶, args 为 [from,to) 范围内的逗号链 */
static int dg_call_node(int callee, int from, int to)
{
    int chain = -1, last = -1, i = from;
    if (callee < 0)
        return -1;
    if (to <= from) {                       /* 空实参表 */
        if (dg_nndo >= DG_NODEN)
            return -1;
        DGNode *nd = &dg_ndo[dg_nndo];
        nd->op = DG_OP_CALL; nd->l = callee; nd->r = -1; nd->x = -1;
        nd->tag = 0; nd->txt[0] = 0;
        return dg_nndo++;
    }
    while (i < to) {                        /* 逐个实参: 递归解析, 逗号链接 */
        int a = dg_expr(&i, 1);
        if (a < 0)
            return -1;
        if (dg_nndo >= DG_NODEN)
            return -1;
        DGNode *cn = &dg_ndo[dg_nndo];
        cn->op = DG_OP_CM; cn->l = a; cn->r = -1; cn->x = -1;
        cn->tag = 0; cn->txt[0] = 0;
        if (last >= 0)
            dg_ndo[last].r = dg_nndo;
        else
            chain = dg_nndo;
        last = dg_nndo++;
        while (i < to && is_space(dg_buf[i]))
            i++;
        if (i < to && dg_tokchar(i) == ',') {
            i++; continue;
        }
        break;
    }
    if (dg_nndo >= DG_NODEN)
        return -1;
    DGNode *nd = &dg_ndo[dg_nndo];
    nd->op = DG_OP_CALL; nd->l = callee; nd->r = chain; nd->x = -1;
    nd->tag = 0; nd->txt[0] = 0;
    return dg_nndo++;
}

static int dg_primary(int *pi)
{
    int i = *pi, n, j;
    while (i < dg_n && is_space(dg_buf[i]))  /* 跳过空格 token */
        i++;
    if (i >= dg_n)
        return -1;
    if (dg_tokchar(i) == '(') {
        int inner;
        i++;
        inner = dg_expr(&i, 1);
        if (inner < 0)
            return -1;
        if (i >= dg_n || dg_tokchar(i) != ')')
            return -1;
        i++;
        while (i < dg_n && is_space(dg_buf[i]))   /* 归一: 定位下一个实 token */
            i++;
        n = inner;
    } else {
        n = dg_newleaf(i);
        if (n < 0)
            return -1;
        i++;
        while (i < dg_n && is_space(dg_buf[i]))
            i++;
    }
    /* 后缀成员访问: `s.id` 折叠进叶文本 (标量成员, tag 清 0, 不误改).
     * i 已指向首个非空 token, 直接判 '.'; 成员名用 next_ns 跳过其后的空格. */
    while (i < dg_n && dg_tokchar(i) == '.'
           && isid(dg_ndo[n].txt[0])) {
        int k = dg_next_ns(i);
        if (k < 0 || !isid(dg_tokchar(k)))
            return -1;
        {
            size_t bl = strlen(dg_ndo[n].txt);
            snprintf(dg_ndo[n].txt + bl, sizeof dg_ndo[n].txt - bl,
                     ".%s", dg_txt[k] ? dg_txt[k] : "");
        }
        dg_ndo[n].tag = 0;       /* 成员是标量, 不再是 operator 类型 */
        i = k + 1;
        while (i < dg_n && is_space(dg_buf[i]))
            i++;
    }
    /* 后缀函数调用: 叶 (标识) 后紧跟 '(' -> 参数改写调用 */
    if (dg_ndo[n].op == 0 && i < dg_n && dg_tokchar(i) == '(') {
        /* 定位与 '(' 配对的 ')' */
        int b = i + 1, d = 1, clo = -1;
        for (; b < dg_n; b++) {
            if (is_space(dg_buf[b]))
                continue;
            if (dg_tokchar(b) == '(')
                d++;
            else if (dg_tokchar(b) == ')') {
                if (--d == 0) { clo = b; break; }
            }
        }
        if (clo < 0)
            return -1;
        n = dg_call_node(n, i + 1, clo);
        if (n < 0)
            return -1;
        i = clo + 1;
        while (i < dg_n && is_space(dg_buf[i]))
            i++;
    }
    /* 后缀自增/自减: `s++` / `s--` (operator 类型). 值语义与 tcc -run 一致:
     * 前后缀统一存回新值并取结果为该新值, 脱糖改写成 operator_inc(s) 同效. */
    if (n >= 0 && i < dg_n) {
        int po = dg_buf[i], poid = dg_opid(po);
        if (poid && dg_op_kind[poid] == 'I') {
            n = dg_mkun(po, n);
            if (n < 0)
                return -1;
            i++;
            while (i < dg_n && is_space(dg_buf[i]))
                i++;
        }
    }
    *pi = i;
    return n;
}

static int dg_mkbin(int optok, int l, int r)
{
    DGNode *nd;
    int tag, id;
    if (l < 0 || r < 0)
        return -1;
    if (dg_nndo >= DG_NODEN)
        return -1;
    /* 类型向上传播: 仅对"二元算术"('B') 且 op 已注册、两侧都为 op 类型时,
     * 二元结果才视为 op 类型; 比较('C') 返回 int (tag=0), 不做传播. */
    id = dg_opid(optok);
    tag = 0;
    if (id && dg_oreg[id] && dg_op_kind[id] == 'B'
        && dg_optagged(l) && dg_optagged(r))
        tag = dg_ndo[l].tag;
    nd = &dg_ndo[dg_nndo];
    nd->op = optok; nd->l = l; nd->r = r; nd->x = -1; nd->tag = tag; nd->txt[0] = 0;
    return dg_nndo++;
}

/* 一元节点 ('!'/'~'): op=unary token, 仅 l 使用. 操作数为 op 类型且该一元已注册
 * 时结果视为同类型 struct (tag 向上传播); 否则 tag=0 (int 取反, verbatim). */
static int dg_mkun(int optok, int l)
{
    DGNode *nd;
    int id;
    if (l < 0)
        return -1;
    if (dg_nndo >= DG_NODEN)
        return -1;
    id = dg_opid(optok);
    nd = &dg_ndo[dg_nndo];
    nd->op = optok;
    nd->l = l;
    nd->r = nd->x = -1;
    nd->tag = (id && dg_oreg[id] && dg_optagged(l)) ? dg_ndo[l].tag : 0;
    nd->txt[0] = 0;
    return dg_nndo++;
}

static int dg_mktern(int c, int t, int e)
{
    DGNode *nd;
    int tag;
    if (c < 0 || t < 0 || e < 0)
        return -1;
    if (dg_nndo >= DG_NODEN)
        return -1;
    tag = (dg_optagged(t) && dg_optagged(e)) ? dg_ndo[t].tag : 0;
    nd = &dg_ndo[dg_nndo];
    nd->op = DG_OP_TERN; nd->l = c; nd->r = t; nd->x = e; nd->tag = tag; nd->txt[0] = 0;
    return dg_nndo++;
}

static int dg_expr(int *pi, int min_prec)
{
    int i = *pi, lhs;
    /* 前缀一元 ! / ~ / ++ / --: 绑定比二元算术更紧. 无论是否 rewrite 都建一元
     * 节点 (命中 operator!/operator~ / operator++/operator-- 改写为函数调用;
     * 普通 int 取反则 verbatim), 由 dg_has_rewrite / dg_pnode 依 tag 与注册态
     * 决定展开. 前缀 `++s`/`--s` 值语义与 tcc -run 一致 (结果=新值). */
    while (i < dg_n && is_space(dg_buf[i]))
        i++;
    if (i < dg_n) {
        int uo = dg_buf[i], uoid = dg_opid(uo);
        if (uoid && (dg_op_kind[uoid] == 'U' || dg_op_kind[uoid] == 'I')) {
            int uop = uo, rp = i + 1;
            int operand = dg_expr(&rp, 4);   /* 高门槛: 一元操作数不吞二元算符 */
            if (operand < 0)
                return -1;
            lhs = dg_mkun(uop, operand);
            if (lhs < 0)
                return -1;
            i = rp;
        } else {
            lhs = dg_primary(&i);
            if (lhs < 0)
                return -1;
        }
    } else {
        lhs = dg_primary(&i);
        if (lhs < 0)
            return -1;
    }
    for (;;) {
        int optok, prec, rhs, rp;
        while (i < dg_n && is_space(dg_buf[i]))  /* 跳过空格 token */
            i++;
        if (i >= dg_n)
            break;
        optok = dg_buf[i];
        prec = dg_prec(optok);
        if (prec <= 0 || prec < min_prec)      /* 非二元/低于门槛 -> 交由上层 */
            break;
        i++;                      /* consume op */
        rp = i;
        rhs = dg_expr(&rp, prec + 1);  /* left-assoc: 右侧需更高级别 */
        if (rhs < 0)
            return -1;
        i = rp;
        lhs = dg_mkbin(optok, lhs, rhs);
    }
    /* 三元 (最低优先级, right-assoc): cond ? then : else.
     * 注意: 循环结束后 i 已停在首个未消费的非空 token 上 (空格已跳过),
     * 直接判 tokchar(i)=='?'. 不能 next_ns 越过 ')'/'/' 误判父级三元. */
    if (min_prec <= 1) {
        if (i < dg_n && dg_tokchar(i) == '?') {
            int t, e;
            i++;
            t = dg_expr(&i, 1);      /* then */
            if (t < 0)
                return -1;
            if (i >= dg_n || dg_tokchar(i) != ':')
                return -1;
            i++;
            e = dg_expr(&i, 1);      /* else (可再含三元) */
            if (e < 0)
                return -1;
            lhs = dg_mktern(lhs, t, e);
        }
    }
    *pi = i;
    return lhs;
}

/* 输出一个节点: 命中 operator 的简单变量运算 -> operator_<op>(l,r), 否则括号原样 */
static void dg_pnode(CString *out, int n)
{
    DGNode *nd;
    if (n < 0)
        return;
    nd = &dg_ndo[n];
    if (nd->op == 0) {
        cstr_cat(out, nd->txt, strlen(nd->txt));
        return;
    }
    if (nd->op == DG_OP_CALL) {
        dg_pnode(out, nd->l);             /* 被调函数名 (不改写) */
        cstr_ccat(out, '(');
        dg_pnode(out, nd->r);             /* 实参逗号链 */
        cstr_ccat(out, ')');
        return;
    }
    if (nd->op == DG_OP_CM) {
        dg_pnode(out, nd->l);
        if (nd->r >= 0) {
            cstr_ccat(out, ',');
            dg_pnode(out, nd->r);
        }
        return;
    }
    if (nd->op == DG_OP_TERN) {
        cstr_ccat(out, '(');
        dg_pnode(out, nd->l);
        cstr_cat(out, ")?(", 3);
        dg_pnode(out, nd->r);
        cstr_cat(out, "):(", 3);
        dg_pnode(out, nd->x);
        cstr_ccat(out, ')');
        return;
    }
    {
        int opid = dg_opid(nd->op);
        if (opid && dg_oreg[opid]) {
            if (dg_op_kind[opid] == 'U' || dg_op_kind[opid] == 'I') {
                /* 一元/自增减: 操作数为 op 类型 -> operator_!(l) / operator_inc(l);
                 * 否则 verbatim !(l) / (l)++. */
                if (dg_optagged(nd->l)) {
                    { const char *__t = get_tok_str(dg_ndo[nd->l].tag, NULL);
                  cstr_cat(out, dg_op_nm_txt(opid, __t), strlen(dg_op_nm_txt(opid, __t))); }
                    cstr_ccat(out, '(');
                    dg_pnode(out, nd->l);
                    cstr_ccat(out, ')');
                } else {
                    const char *ot = dg_optxt(nd->op);
                    cstr_cat(out, ot, strlen(ot));
                    cstr_ccat(out, '(');
                    dg_pnode(out, nd->l);
                    cstr_ccat(out, ')');
                }
                return;
            }
            /* 'B' 二元 / 'C' 比较: 两侧均 op 类型 -> operator_X(l,r) */
            if (dg_optagged(nd->l) && dg_optagged(nd->r)) {
                { const char *__t = get_tok_str(dg_ndo[nd->l].tag, NULL);
                  cstr_cat(out, dg_op_nm_txt(opid, __t), strlen(dg_op_nm_txt(opid, __t))); }
                cstr_ccat(out, '(');
                dg_pnode(out, nd->l);
                cstr_ccat(out, ',');
                dg_pnode(out, nd->r);
                cstr_ccat(out, ')');
                return;
            }
        }
        /* verbatim: (l op r). 多字符比较/自增减用 dg_optxt 完整还原 */
        cstr_ccat(out, '(');
        dg_pnode(out, nd->l);
        cstr_ccat(out, ' ');
        {
            const char *ot = dg_optxt(nd->op);
            cstr_cat(out, ot, strlen(ot));
        }
        cstr_ccat(out, ' ');
        dg_pnode(out, nd->r);
        cstr_ccat(out, ')');
    }
}

/* --------------------------------------------------------------------- */
/* model 泛型脱糖 (token 层, struct/union 模板): 在实例化处展开为合成 typedef.
 *
 * 语法 (TCC 扩展):  model struct Name(T, int N, ...) { body };
 *                  model union  Name(T)             { body };
 * 实例化出现即改写:  Name(float, 4*3) var   ->   <synth> var
 * 其中 <synth> 为合成 typedef 名 (Name_float_12), 首次用到时补发
 *                  typedef struct <synth> { <body 参数替换> } <synth>;
 * const 参数 (int N 声明) 实参做常量求值 (2+2->4) 用于合成名去重;
 * body 内嵌套实例 (如 Node(T)* next) 递归展开.
 * 函数泛型 (model (T) ret name(...) {...}) 不在此范围: 定义被丢弃, 相应调用点
 * clang 会报类型错误 -> 已知限制 (见 docs/desugar.md roadmap).
 */
#define DG_MODEL_MAXP 32
/* DgModelDef 已前向声明; 此处补全结构体定义 */
struct DgModelDef {
    char ntxt[64];               /* 模板名文本 (≥26: stl_unordered_map_contains 等长名不可截断到 24) */
    int  kind;                   /* 'S' struct, 'U' union, 'F' 函数(丢弃) */
    int  nparams;
    char pk[DG_MODEL_MAXP];      /* 't' 类型参数 / 'c' 整型常量参数 */
    char pn[DG_MODEL_MAXP][128];  /* 参数名 */
    DgTk *dt; int dtn;           /* 自持定义 token 序列 (finish 从 dgm 拷贝; 重放直读, 免 join/split) */
    int bbody, ebody;            /* 定义体 span [bbody,ebody), 不含外层花括号 (相对 dt) */
    int bret, eret;              /* 函数泛型: 返回类型 span */
    int bfp,  efp;               /* 函数泛型: 形参 span, 含左右括号 */
    struct DgModelDef *next;
};

/* 清空 model 收集态 (dg_reset 调用; 结构体完整后定义) */
static void dg_model_reset_impl(void)
{
    DgModelDef *m = dg_model_def, *nx;
    int i;
    while (m) {
        nx = m->next;
        if (m->dt) {
            for (i = 0; i < m->dtn; i++)
                if (m->dt[i].txt)
                    tcc_free(m->dt[i].txt);
            tcc_free(m->dt);
        }
        tcc_free(m);
        m = nx;
    }
    dg_model_def = NULL;
    dg_model_nout = 0;
    for (i = 0; i < dg_fout_typed; i++)
        if (dg_fout_type[i]) {
            tcc_free(dg_fout_type[i]);
            dg_fout_type[i] = NULL;
        }
    dg_fout_typed = 0;
    dg_mc = 0;
    dg_mbr = 0;
    dg_msemi = 0;
    dgm_free();
    /* 待发射的函数泛型定义体 */
    while (dg_fdefs) {
        DgModelFDef *fp = dg_fdefs;
        dg_fdefs = fp->next;
        if (fp->synth)   tcc_free(fp->synth);
        if (fp->proto)   tcc_free(fp->proto);
        if (fp->def)     tcc_free(fp->def);
        tcc_free(fp);
    }
    /* 函数泛型累积 typedef */
    if (dg_fout_td_init) {
        cstr_free(&dg_fout_td);
        dg_fout_td_init = 0;
    }
    memset(&dg_fout_td, 0, sizeof dg_fout_td);
}

static void dg_model_emit(TCCState *s1, const DgModelDef *m, char av[][64]);
static void dg_model_expand_src(TCCState *s1, const DgModelDef *m, char av[][64],
                                int bl, int el, CString *out);
static void dg_fdefs_typedefs(TCCState *s1);
static void dg_fdefs_funcs(TCCState *s1);
static void dg_reflect_emit(TCCState *s1);

static DgModelDef *dg_model_find(const char *nm)
{
    DgModelDef *m;
    for (m = dg_model_def; m; m = m->next)
        if (!strcmp(m->ntxt, nm))
            return m;
    return NULL;
}

/* ---- model 定义 token 序列服务: 跨行累积, 结构保真(去文本尖峰) ----
 * 取代旧的 CString dg_mb + 空格拼文本 + dg_splitw 二次切词:
 * 定义 token 直接驻留在此数组, 括号配对/finish 定位全在 token 上做,
 * 摆脱 DG_TOKENCAP_N 上限与 256KB 栈缓冲, 也保留分隔符身份(不压平成空格). */
static void dgm_add(int t, const char *txt)
{
    size_t n;
    char *p;
    if (dgm_n >= dgm_cap) {
        dgm_cap = dgm_cap ? dgm_cap * 2 : 256;
        dgm = tcc_realloc(dgm, (size_t)dgm_cap * sizeof(DgTk));
    }
    n = txt ? strlen(txt) : 0;
    p = tcc_malloc(n + 1);
    if (n) memcpy(p, txt, n);
    p[n] = 0;
    dgm[dgm_n].t = t;
    dgm[dgm_n].txt = p;
    dgm_n++;
}
/* 释放整个 token 序列(含各文本快照) */
static void dgm_free(void)
{
    int i;
    if (!dgm)
        return;
    for (i = 0; i < dgm_n; i++)
        if (dgm[i].txt)
            tcc_free(dgm[i].txt);
    tcc_free(dgm);
    dgm = NULL; dgm_n = dgm_cap = 0;
}
/* dgm[i] 是否等于给定词/单字符分隔符 (匹配 token 值或文本) */
static int dgm_is(int i, const char *word, int ch)
{
    if (dgm[i].t == ch)
        return 1;
    return dgm[i].txt && !strcmp(dgm[i].txt, word);
}
/* dgm[i] 的文本 (无则空串) */
static const char *dgm_str(int i)
{
    return dgm[i].txt ? dgm[i].txt : "";
}
/* 空格分隔文本 -> 单词数组 */
#define DG_TOKENCAP_N 2048   /* model 体 token 上限: 宏展开的大函数体(如 set/at)远超 512 会被截断 */
static int dg_splitw(const char *s, char (*w)[128], int max)
{
    int n = 0;
    while (s && *s) {
        int j;
        while (*s == ' ') s++;
        if (!*s) break;
        j = 0;
        while (*s && *s != ' ' && j < 127) w[n][j++] = *s++;
        w[n][j] = 0;
        n++;
        if (n >= max) break;
    }
    return n;
}

/* 是否为整型类型关键字 (常量参数判定: "int R") */
static int dg_w_intkey(const char *w)
{
    return !strcmp(w, "int") || !strcmp(w, "char") || !strcmp(w, "short")
        || !strcmp(w, "long") || !strcmp(w, "unsigned") || !strcmp(w, "signed");
}

/* 首尾空白就地裁掉, 返回指向正文的指针 (原地修改, 终有 NUL) */
static const char *dg_trimstr(char *s)
{
    char *p = s;
    size_t len;
    while (*p == ' ' || *p == '\t' || *p == '\n')
        p++;
    len = strlen(p);
    while (len && (p[len - 1] == ' ' || p[len - 1] == '\t' || p[len - 1] == '\n'))
        p[--len] = 0;
    return p;
}

/* 从 token 序列 [ps,pe] 解析 model 类型参数段 ('(' ')' 之间, 以顶层 ',' 分段;
 * 段含整型关键字=整型常量参数, 否则为类型参数). struct 与 函数泛型共用. */
static void dg_model_parse_params(DgModelDef *m, int ps, int pe)
{
    int seg = ps, k;
    m->nparams = 0;
    for (k = ps; k <= pe; k++) {
        int isconst = 0, j;
        if (k != pe && !dgm_is(k, ",", ','))
            continue;
        if (k == seg) { seg = k + 1; continue; }   /* 空段 */
        for (j = seg; j < k; j++)
            if (dg_w_intkey(dgm_str(j))) { isconst = 1; break; }
        if (m->nparams >= DG_MODEL_MAXP)
            break;
        m->pk[m->nparams] = isconst ? 'c' : 't';
        if (isconst) {
            int li = k - 1;
            while (li >= seg && dg_w_intkey(dgm_str(li))) li--;
            snprintf(m->pn[m->nparams], 128, "%s", (li >= seg) ? dgm_str(li) : dgm_str(k - 1));
        } else {
            snprintf(m->pn[m->nparams], 128, "%s", dgm_str(k - 1));
        }
        m->nparams++;
        seg = k + 1;
    }
}

/* ---- 极小整型常量求值器 (空格分隔 token 文本) ---- */
static long long dg_cint_expr(const char **pp);
static long long dg_cint_factor(const char **pp)
{
    const char *p = *pp, *q;
    char b[32];
    int j = 0;
    long long v;
    while (*p == ' ') p++;
    if (*p == '(') {
        p++; *pp = p; v = dg_cint_expr(pp); p = *pp;
        while (*p == ' ') p++; if (*p == ')') p++;
        *pp = p; return v;
    }
    if (*p == '-') { p++; *pp = p; return -dg_cint_factor(pp); }
    if (*p == '+') { p++; *pp = p; return dg_cint_factor(pp); }
    q = p;
    while (isid((unsigned char)*q) || isnum((unsigned char)*q)) q++;
    j = (int)(q - p); if (j > 31) j = 31;
    memcpy(b, p, j); b[j] = 0;
    *pp = q;
    return strtoll(b, NULL, 0);
}
static long long dg_cint_term(const char **pp)
{
    long long v = dg_cint_factor(pp);
    const char *p = *pp;
    for (;;) {
        char c; long long r, base = v;
        while (*p == ' ') p++; c = *p;
        if (c != '*' && c != '/' && c != '%') { *pp = p; return v; }
        p++; *pp = p; r = dg_cint_factor(pp); p = *pp;
        if (c == '*') v = base * r;
        else if (c == '/') v = r ? base / r : 0;
        else v = r ? base % r : 0;
    }
}
static long long dg_cint_expr(const char **pp)
{
    long long v = dg_cint_term(pp);
    const char *p = *pp;
    for (;;) {
        char c; long long r, base = v;
        while (*p == ' ') p++; c = *p;
        if (c != '+' && c != '-') { *pp = p; return v; }
        p++; *pp = p; r = dg_cint_term(pp); p = *pp;
        v = (c == '+') ? base + r : base - r;
    }
}
static long long dg_cint(const char *txt)
{
    const char *p = txt;
    return dg_cint_expr(&p);
}

/* 把已收集的 dgm[0..n) 拷贝为本定义的持久 token 序列 (dt)。
 * 此后 finish 记录的 span 相对 dt, 重放直接迭代 dt 区间, 不再 join 成串 + split 回切. */
static void dg_model_copy_tokens(DgModelDef *m, int n)
{
    int i;
    m->dt = tcc_malloc((size_t)(n ? n : 1) * sizeof(DgTk));
    m->dtn = 0;
    for (i = 0; i < n; i++) {
        const char *s = dgm[i].txt ? dgm[i].txt : "";
        size_t sl = strlen(s);
        char *p = tcc_malloc(sl + 1);
        if (sl) memcpy(p, s, sl);
        p[sl] = 0;
        m->dt[m->dtn].t = dgm[i].t;
        m->dt[m->dtn].txt = p;
        m->dtn++;
    }
}

/* 收集结束: 解析 dgm token 序列为一个 model 定义并登记.
 * struct/union 泛型: 实例化处展开为合成 typedef;
 * 函数泛型 `model (T, int N) RET name(args) { body }`: 登记为 'F' 模板,
 * 实例化点(调用区)改写为合成函数名, 定义体延迟到文件末尾发射. */
static void dg_model_finish(void)
{
    int n = dgm_n;
    int i, openb = -1, closeb = -1;
    DgModelDef *m;

    if (n < 3)
        return;

    if (dgm_is(1, "(", '(')) {
        /* ===== 函数泛型: w[1]='(' 类型参数表. 布局:
         * model ( T , int N ) RET name ( args ) { body }                 */
        int pclose = -1;
        m = tcc_malloc(sizeof *m);
        memset(m, 0, sizeof *m);
        m->kind = 'F';
        for (i = 2; i < n; i++)
            if (dgm_is(i, ")", ')')) { pclose = i; break; }
        if (pclose < 0) { tcc_free(m); return; }
        dg_model_parse_params(m, 2, pclose);
        /* 函数名/参数表/体: 锚点式解析 — 体开 "{" = 参数表右括号 ")" 之后紧随的 "{",
         * 再前向花括号平衡扫描找其配对 "}". 不再从尾部反向配对: collect 在宏体下
         * 曾提前截断, 反向会把 openb 错配到内层块 (set 截断 root cause). */
        {
            int fp = -1, fname = -1, fpclose = -1, k;
            /* 函数名参数表 "(": 其匹配 ")" 之后紧跟 "{" 者即为函数体参数表 */
            for (i = pclose + 1; i < n; i++) {
                if (dgm_is(i, "(", '(')) {
                    int d = 1, j = i + 1;
                    for (; j < n; j++) {
                        if (dgm_is(j, "(", '(')) d++;
                        else if (dgm_is(j, ")", ')') && --d == 0) break;
                    }
                    if (j < n && j + 1 < n && dgm_is(j + 1, "{", '{')) { fp = i; fpclose = j; break; }
                }
            }
            if (fp < 0 || fp - 1 <= pclose) { tcc_free(m); return; }
            fname = fp - 1;
            openb = fpclose + 1;                      /* 体开 "{" */
            closeb = -1;
            {
                int d = 1;
                for (k = openb + 1; k < n; k++) {
                    if (dgm_is(k, "{", '{')) d++;
                    else if (dgm_is(k, "}", '}') && --d == 0) { closeb = k; break; }
                }
            }
            if (closeb < 0)   /* 体未闭合 = 上游截断, 弃 */
                { tcc_free(m); return; }
            snprintf(m->ntxt, sizeof m->ntxt, "%s", dgm_str(fname));
            m->bret  = pclose + 1;  m->eret  = fname;
            m->bfp   = fp;          m->efp   = fpclose + 1;
            m->bbody = openb + 1;   m->ebody = closeb;
            dg_model_copy_tokens(m, n);
        }
        m->next = dg_model_def;
        dg_model_def = m;
        return;
    }

    for (i = 0; i < n; i++) if (dgm_is(i, "{", '{')) { openb = i; break; }
    if (openb < 0)
        return;
    for (i = n - 1; i > openb; i--) if (dgm_is(i, "}", '}')) { closeb = i; break; }
    if (closeb < 0)
        return;

    m = tcc_malloc(sizeof *m);
    memset(m, 0, sizeof *m);
    m->kind = !strcmp(dgm_str(1), "union") ? 'U' : 'S';
    snprintf(m->ntxt, sizeof m->ntxt, "%s", dgm_str(2));

    /* 参数: '(' ')' 之间按顶层 ',' 分段; 段含整型关键字=常量参数 */
    {
        int openp = -1, closep = -1;
        for (i = 0; i < n; i++) if (dgm_is(i, "(", '(')) { openp = i; break; }
        if (openp < 0) { tcc_free(m); return; }
        for (i = openp + 1; i < n; i++) if (dgm_is(i, ")", ')')) { closep = i; break; }
        if (closep < 0) { tcc_free(m); return; }
        dg_model_parse_params(m, openp + 1, closep);
    }

    /* body: 外层花括号之间 token span; 自持 token 序列供重放直读 */
    m->bbody = openb + 1;  m->ebody = closeb;
    dg_model_copy_tokens(m, n);
    m->next = dg_model_def;
    dg_model_def = m;
}

/* 合成 typedef 名: Name_Arg1_Arg2... (类型参数去符号, 常量参数用值去重) */
/* 合成类型名: 写入调用方提供的缓冲区 (此前返回 static buf, 嵌套 emit 递归调用
 * dg_model_synth 会覆写之, 导致外层 fprintf 读到被污染的名字). */
static void dg_model_synth(const DgModelDef *m, char av[][64], char *buf, int osize)
{
    CString n;
    int i;
    cstr_new(&n);
    cstr_cat(&n, m->ntxt, strlen(m->ntxt));
    for (i = 0; i < m->nparams; i++) {
        const char *q;
        cstr_ccat(&n, '_');
        if (m->pk[i] == 'c') {
            char tmp[24];
            snprintf(tmp, sizeof tmp, "%lld", dg_cint(av[i]));
            cstr_cat(&n, tmp, strlen(tmp));
        } else {
            for (q = av[i]; *q; q++) {
                char c = *q;
                if (!(isid((unsigned char)c) || isnum((unsigned char)c)))
                    c = '_';
                cstr_ccat(&n, c);
            }
        }
    }
    cstr_ccat(&n, 0);
    snprintf(buf, osize, "%s", n.data);
    cstr_free(&n);
}

/* 从单词数组中解析一个 model 实例的实参 (i 指向 '('), 返回实参数和终点下标 */
static int dg_model_av_from_words(TCCState *s1, char (*w)[128], int n,
                                  int i, const DgModelDef *m,
                                  char av[][64], int *pend)
{
    int idx = i + 2, pos = 0, done = 0;   /* i->模板名, i+1='(', i+2 起为实参内容 */
    if (idx > n)
        return 0;
    while (!done) {
        CString a;
        int d = 0, begin = 1;
        cstr_new(&a);
        for (; idx < n; idx++) {
            const char *tok = w[idx];
            if (!strcmp(tok, "(")) {
                d++; cstr_ccat(&a, '('); begin = 0;
            } else if (!strcmp(tok, ")")) {
                /* 0 深度遇到 ')' = 本实例闭合 (实例自己的 '(' 已在 idx=i+2 前跳过,
                 * 不再包进实参文本, 保证 `Node(int)` 的实参为干净的 `int` 而非 `( int`. */
                if (d == 0) { done = 1; idx++; break; }
                d--;
                cstr_ccat(&a, ')'); begin = 0;
            } else if (!strcmp(tok, ",") && d == 0) {
                idx++; break;
            } else {
                DgModelDef *nn = dg_model_find(tok);
                int nxj = idx + 1;
                if (nn && nn->kind != 'F' && nxj < n && !strcmp(w[nxj], "(")) {
                    char nav[DG_MODEL_MAXP][64];
                    int ne;
                    if (dg_model_av_from_words(s1, w, n, idx, nn, nav, &ne)) {
                        char sn[384];
                        dg_model_synth(nn, nav, sn, sizeof sn);
                        dg_model_emit(s1, nn, nav);
                        if (!begin) cstr_ccat(&a, ' ');
                        cstr_cat(&a, sn, strlen(sn));
                        begin = 0;
                        idx = ne - 1;
                        continue;
                    }
                }
                if (!begin)
                    cstr_ccat(&a, ' ');
                cstr_cat(&a, tok, strlen(tok));
                begin = 0;
            }
        }
        if (pos < DG_MODEL_MAXP) {
            int q = 0, e = a.size, L;
            while (q < e && a.data[q] == ' ') q++;
            while (e > q && a.data[e - 1] == ' ') e--;
            L = (e - q < 63) ? (e - q) : 63;
            memcpy(av[pos], a.data + q, L);
            av[pos][L] = 0;
            pos++;
        }
        cstr_free(&a);
        if (done || pos > DG_MODEL_MAXP)
            break;
    }
    if (pos != m->nparams)
        return 0;
    *pend = idx;
    return 1;
}

/* 展开任意源文本 (body/ret/fparams): 先做参数名替换, 再解析体内嵌套实例并
 * 递归发射. 嵌套 struct/union 实例统一由 dg_model_emit 去重累积 typedef 到
 * 文件作用域全局池 dg_fout_td, 引用以 `struct <n>`/`union <n>` tag 形式内联,
 * 使函数泛型定义自包含 (语句级与函数定义级走同一路径). */
static void dg_model_expand_src(TCCState *s1, const DgModelDef *m, char av[][64],
                                int bl, int el, CString *out)
{
    char w[DG_TOKENCAP_N][128], w2[DG_TOKENCAP_N][128];
    int n = 0, k, i, n2 = 0, pi;
    /* 直接迭代本定义持久 token 序列 dt[bl,el), 免 dg_tok_join 压串 + dg_splitw 回切 */
    for (k = bl; k < el && n < DG_TOKENCAP_N; k++) {
        const char *s = (m->dt[k].txt) ? m->dt[k].txt : "";
        if (!*s)
            continue;
        snprintf(w[n++], 128, "%s", s);
    }
    /* phase1: 参数名替换为实参文本 */
    for (i = 0; i < n; i++) {
        int hit = 0, j;
        for (j = 0; j < m->nparams; j++) {
            if (!strcmp(w[i], m->pn[j])) {
                pi = j; hit = 1; break;
            }
        }
        if (hit && m->pk[pi] == 'c') {
            char tmp[24];
            snprintf(tmp, sizeof tmp, "%lld", dg_cint(av[pi]));
            snprintf(w2[n2++], 128, "%s", tmp);
        } else if (hit) {
            /* 类型参数: 拆词填入 (av 可能含空格) */
            char x[DG_TOKENCAP_N][128];
            int xn = dg_splitw(av[pi], x, DG_TOKENCAP_N), k;
            for (k = 0; k < xn; k++)
                snprintf(w2[n2++], 128, "%s", x[k]);
        } else {
            snprintf(w2[n2++], 128, "%s", w[i]);
        }
    }
    /* phase2: 嵌套模型实例 -> tag/合成名 + 发射其 typedef */
    for (i = 0; i < n2;) {
        /* 泛型体自递归: 当前函数泛型自身名后跟 '(' → 裸调用绑定到本实例合成名.
         * (如 `stl_qsort(a,lo,j)` → `stl_qsort_<T>(a,lo,j)` 以匹配已落地定义) */
        if (dg_fdef_name && !strcmp(w2[i], dg_fdef_name) &&
            i + 1 < n2 && !strcmp(w2[i + 1], "(")) {
            if (out->size && out->data[out->size - 1] != ' ')
                cstr_ccat(out, ' ');
            cstr_cat(out, dg_fdef_synth, strlen(dg_fdef_synth));
            i++;
            continue;
        }
        DgModelDef *nn = dg_model_find(w2[i]);
        if (nn && nn->kind != 'F' && i + 1 < n2 && !strcmp(w2[i + 1], "(")) {
            char na[DG_MODEL_MAXP][64];
            int ne;
            if (dg_model_av_from_words(s1, w2, n2, i, nn, na, &ne)) {
                char sn[384];
                dg_model_synth(nn, na, sn, sizeof sn);
                /* 统一走 dg_model_emit: 递归展开嵌套 + 去重累积 typedef 到全局
                 * dg_fout_td (语句级与函数定义级同一路径, 消除重复实现) */
                dg_model_emit(s1, nn, na);
                if (out->size && out->data[out->size - 1] != ' ')
                    cstr_ccat(out, ' ');
                /* 体内嵌套实例用 tag 形式 (`struct Link_int`/`union Val_int`):
                 * typedef 名要等花括号闭合后才进作用域, 自引成员 `Node(T)* next`
                 * 若写成 `Node_int* next` 会触发 gcc "unknown type name". */
                const char *tagpre = (nn->kind == 'U') ? "union " : "struct ";
                cstr_cat(out, tagpre, strlen(tagpre));
                cstr_cat(out, sn, strlen(sn));
                i = ne;
                continue;
            }
        }
        if (out->size && out->data[out->size - 1] != ' ')
            cstr_ccat(out, ' ');
        cstr_cat(out, w2[i], strlen(w2[i]));
        i++;
    }
}

static void dg_model_expand(TCCState *s1, const DgModelDef *m,
                            char av[][64], CString *out)
{
    dg_model_expand_src(s1, m, av, m->bbody, m->ebody, out);
}

static int dg_model_out_has(const char *s)
{
    int i;
    for (i = 0; i < dg_model_nout; i++)
        if (!strcmp(dg_model_out[i], s))
            return 1;
    return 0;
}

/* 发射(并去重)合成 typedef. mark-before-expand 防递归自引无限展开.
 * 统一累积到文件作用域全局池 dg_fout_td(语句级不再直接打印到 ppfp):
 *  - 消除"文件作用域"与"语句级块作用域"的重复 typedef(redefinition);
 *  - 确保 main 内实例化的嵌套类型(STL_Pair_int_int 等)先在 fdefs 区定义,
 *    使 STL_Vector_STL_Pair_int_int 等引用者在依赖类型之后. */
static void dg_model_emit(TCCState *s1, const DgModelDef *m, char av[][64])
{
    char synth[384];
    CString body;
    int k;

    dg_model_synth(m, av, synth, sizeof synth);
    if (dg_model_out_has(synth))
        return;
    if (!dg_fout_td_init) {
        cstr_new(&dg_fout_td);
        dg_fout_td_init = 1;
    }
    {   /* 若该合成名已在文件作用域落地, 只登记引用, 不再重复累积 */
        for (k = 0; k < dg_fout_typed; k++)
            if (!strcmp(dg_fout_type[k], synth)) {
                if (dg_model_nout < 512)
                    dg_model_out[dg_model_nout++] = tcc_strdup(synth);
                return;
            }
    }
    if (dg_fout_typed < 512)
        dg_fout_type[dg_fout_typed++] = tcc_strdup(synth);
    if (dg_model_nout < 512)
        dg_model_out[dg_model_nout++] = tcc_strdup(synth);
    if (dg_fout_td.size && dg_fout_td.data[dg_fout_td.size - 1] != '\n')
        cstr_ccat(&dg_fout_td, '\n');
    cstr_new(&body);
    dg_model_expand(s1, m, av, &body);
    if (body.size) {
        cstr_ccat(&body, 0);       /* cstr_cat 不写 NUL, 补终止符供 strlen */
        cstr_cat(&dg_fout_td, "typedef ", 8);
        cstr_cat(&dg_fout_td, m->kind == 'U' ? "union " : "struct ",
                 m->kind == 'U' ? 6 : 7);
        cstr_cat(&dg_fout_td, synth, strlen(synth));
        cstr_cat(&dg_fout_td, " { ", 3);
        cstr_cat(&dg_fout_td, body.data, strlen(body.data));
        cstr_cat(&dg_fout_td, " } ", 3);
        cstr_cat(&dg_fout_td, synth, strlen(synth));
        cstr_cat(&dg_fout_td, ";\n", 2);
    }
    cstr_free(&body);
}

/* 登记一个函数泛型实例 (同类型实参去重, 每次调用仅登记一次):
 * 展开返回/形参/体到自包含定义文本; 嵌套 struct 的 typedef 累积进 dg_fout_td
 * (文件作用域, 全局去重), ref 用 `struct <sn>` tag 形式.
 * 定义在 EOF 处 (dg_fdefs_flush) 发射且物理置于主体之前, 故 main 内调用点在
 * 编译时先见到定义(即可充当原型), 无需在表达式内插入声明. */
/* ============ 泛型体 operator 改写 (P1) ============
 * 处理函数泛型展开体里的 `a < b` / `a == b` 等: 当该实例的类型实参
 * (av[0]) 是已注册 operator 的类型(如 struct Cmp, opbase="Cmp")时,
 * 泛型体中对 op 类型操作数的比较/判等改写为 operator_X(l,r) 调用;
 * 否则(如 T=int)原样透传, 不动 int 原生比较.
 *
 * 变量类型取自 fparams 形参 与 body 内 `opbase * v` / `opbase v` 声明;
 * 操作数判定: OP 值变量, 或 OP 指针下标 `v[expr]`(解引用). 局限: 成员
 * (`self->x`) / 一元解引用(`*p`) / 复杂嵌套暂不覆盖, 见 docs/desugar.md. */
#define DG_OPTYPN 64
static char dg_optyp_name[DG_OPTYPN][40];   /* 已注册 operator 的操作数基础类型名 */
static int dg_optyp_n;

/* 类型文本 -> 基础名 (去 struct/union 前缀; 取首标识符) */
static const char *dg_typbase(const char *t)
{
    static char b[40];
    int i = 0;
    if (!t)
        return "";
    if (!strncmp(t, "struct", 6) && (t[6] == ' ' || !t[6]))
        { t += 6; while (*t == ' ') t++; }
    else if (!strncmp(t, "union", 5) && (t[5] == ' ' || !t[5]))
        { t += 5; while (*t == ' ') t++; }
    while (*t && *t != ' ' && *t != '*' && *t != '[' && i < 39)
        b[i++] = *t++;
    b[i] = 0;
    return b;
}
/* operator 定义行(i 为 operator/`operator_<wrd>` token)的操作数基础类型名,
 * 用于定义端改名 operator_<wrd>_<type> 与调用端对齐 */
static const char *dg_op_def_typ(int i)
{
    static char tn[48];
    int j, d = 0;
    tn[0] = 0;
    for (j = i + 1; j < dg_n; j++) {
        if (is_space(dg_buf[j]))
            continue;
        if (dg_tokchar(j) == '(') { d = 1; continue; }
        if (d > 0) {
            const char *tj;
            if (dg_tokchar(j) == ')')
                break;
            if (!isid((unsigned char)dg_tokchar(j)))
                continue;
            tj = dg_txt[j] ? dg_txt[j] : "";
            if (!strcmp(tj, "struct") || !strcmp(tj, "union")
                || !strcmp(tj, "const") || !strcmp(tj, "unsigned")
                || !strcmp(tj, "signed"))
                continue;
            snprintf(tn, sizeof tn, "%s", dg_typbase(tj));
            break;
        }
    }
    return tn;
}

static void dg_optyp_add(const char *tn)
{
    const char *b = dg_typbase(tn);
    int i;
    if (!b || !*b)
        return;
    for (i = 0; i < dg_optyp_n; i++)
        if (!strcmp(dg_optyp_name[i], b))
            return;
    if (dg_optyp_n < DG_OPTYPN) {
        snprintf(dg_optyp_name[dg_optyp_n], 40, "%s", b);
        dg_optyp_n++;
    }
}
static int dg_optyp_has(const char *tn)
{
    const char *b = dg_typbase(tn);
    int i;
    if (!b || !*b)
        return 0;
    for (i = 0; i < dg_optyp_n; i++)
        if (!strcmp(dg_optyp_name[i], b))
            return 1;
    return 0;
}

static int dg_gvget(char gv[][40], const int *gvk, int gvc, const char *nm)
{
    int q;
    for (q = 0; q < gvc; q++)
        if (!strcmp(gv[q], nm))
            return gvk[q];
    return 0;
}

/* 把 [from,to] 区间单词用单空格拼入 buf (跳过空词), 返回拼接长度; 超界安全截断.
 * 供二元运算改写的左右操作数文本还原 (lbt/rbt) 共用. */
static int dg_join_words(char (*w2)[128], int from, int to, char *buf, int bufsz)
{
    int jj, r = 0;
    for (jj = from; jj <= to; jj++) {
        int L;
        if (!w2[jj][0])
            continue;
        if (r)
            buf[r++] = ' ';
        L = (int)strlen(w2[jj]);
        if (r + L >= bufsz)
            break;
        memcpy(buf + r, w2[jj], (size_t)L);
        r += L;
        buf[r] = 0;
    }
    return r;
}

/* 从下标闭 ']' 位置 end 向前找配对的 '[' 并返回其紧邻之前的基址词索引
 * (跳过空词); 无匹配 '[' 或前方无词时返回 -1. lo 为扫描下界 (语句起点).
 * 供 obj_start / op_left / op_right 三处下标操作数基址识别共用. */
static int dg_subscript_base(char (*w2)[128], int end, int lo)
{
    int dep = 1, q = end - 1;
    for (; q >= lo; q--) {
        while (q >= lo && !w2[q][0]) q--;
        if (q < lo)
            break;
        if (!strcmp(w2[q], "]")) dep++;
        else if (!strcmp(w2[q], "[")) { dep--; if (!dep) { q--; break; } }
    }
    while (q >= lo && !w2[q][0]) q--;
    return (q < lo) ? -1 : q;
}

/* '.' / '->' 之前对象基址起始词索引: 支持 `d [ .. ]`(下标) / 访问器链(递归) /
 * 标识符. `->` 与 `.` 同等待遇: `e -> key` 与 `a . b . key` 都收全对象基址,
 * 避免左操作数只取到成员名而把 `e ->` 留在改写调用之外 (左值漂移). */
static int dg_gbody_obj_start(char (*w2)[128], int dot)
{
    int p = dot - 1;
    while (p >= 0 && !w2[p][0]) p--;
    if (p < 0) return dot;
    if (!strcmp(w2[p], "]")) {
        int b = dg_subscript_base(w2, p, 0);
        return b < 0 ? p : b;   /* 无基址回退 ']' (非法表达, 实际不可达) */
    }
    if (isid((unsigned char)w2[p][0])) {    /* 标识符: 沿访问器链向前收全基址 */
        int q = p - 1;
        while (q >= 0 && !w2[q][0]) q--;
        if (q >= 0 && (!strcmp(w2[q], ".") || !strcmp(w2[q], "->")))
            return dg_gbody_obj_start(w2, q);
        return p;
    }
    if (!strcmp(w2[p], ".") || !strcmp(w2[p], "->"))
        return dg_gbody_obj_start(w2, p);
    return p;                       /* 普通标识符 */
}

/* 左侧操作数: k 为比较运算符词索引, 回写 [ls,le]. 返回 1=OP 类型命中 */
static int dg_gbody_op_left(char (*w2)[128], int k,
                            char gv[][40], const int *gvk, int gvc,
                            int *ls, int *le)
{
    int a = k - 1, end;
    while (a >= 0 && !w2[a][0]) a--;
    if (a < 0) return 0;
    end = a;
    if (!strcmp(w2[end], "]")) {           /* 下标: 基为 OP 指针 */
        int q = dg_subscript_base(w2, end, 0);
        if (q >= 0 && isid((unsigned char)w2[q][0])
            && dg_gvget(gv, gvk, gvc, w2[q]) == 2) { *ls = q; *le = end; return 1; }
        return 0;
    }
    if (isid((unsigned char)w2[end][0])) {
        int gv1 = dg_gvget(gv, gvk, gvc, w2[end]);
        if (gv1 == 2) {                   /* `* p` 一元解引用 */
            int q = end - 1;
            while (q >= 0 && !w2[q][0]) q--;
            if (q >= 0 && !strcmp(w2[q], "*")) { *ls = q; *le = end; return 1; }
            return 0;
        }
        if (gv1 == 1) {                   /* OP 值, 可带 '.成员' / '->成员' 前缀 */
            *le = end; *ls = end;
            { int dot = end - 1;
              while (dot >= 0 && !w2[dot][0]) dot--;
              if (dot >= 0 && (!strcmp(w2[dot], ".") || !strcmp(w2[dot], "->"))) {
                  *ls = dg_gbody_obj_start(w2, dot);
                  { int q = *ls - 1; while (q >= 0 && !w2[q][0]) q--;
                    if (q >= 0 && !strcmp(w2[q], "*")) *ls = q; }
              } }
            return 1;
        }
        return 0;
    }
    return 0;
}

/* 右侧操作数: k 为比较运算符词索引, 回写 [rs,re]. 返回 1=OP 类型命中 */
static int dg_gbody_op_right(char (*w2)[128], int N, int k,
                             char gv[][40], const int *gvk, int gvc,
                             int *rs, int *re)
{
    int b = k + 1, i, end;
    while (b < N && !w2[b][0]) b++;
    if (b >= N) return 0;
    end = b - 1;
    i = b;
    while (i < N) {                       /* 收集到语句分隔符 */
        if (!w2[i][0]) { i++; continue; }
        if (isid((unsigned char)w2[i][0]) || isnum((unsigned char)w2[i][0]))
            { end = i; i++; continue; }
        if (!strcmp(w2[i], ".") || !strcmp(w2[i], "->"))
            { i++; continue; }
        if (!strcmp(w2[i], "*")) { i++; continue; }
        if (!strcmp(w2[i], "[")) {
            int dep = 1, q = i + 1;
            for (; q < N; q++) {
                if (!w2[q][0]) continue;
                if (!strcmp(w2[q], "[")) dep++;
                else if (!strcmp(w2[q], "]")) { dep--; if (!dep) break; }
            }
            end = q;                  /* 下标值位置 (']'), 后续 .member 可覆盖 */
            i = q + 1;
            while (i < N && !w2[i][0]) i++;
            continue;
        }
        break;
    }
    if (end < b) return 0;
    /* 尾词判定 OP 类型 */
    if (!strcmp(w2[end], "]")) {          /* 下标: 基为 OP 指针 */
        int q = dg_subscript_base(w2, end, b);
        if (q >= b && isid((unsigned char)w2[q][0])
            && dg_gvget(gv, gvk, gvc, w2[q]) == 2) { *rs = b; *re = end; return 1; }
        return 0;
    }
    if (isid((unsigned char)w2[end][0])) {
        if (dg_gvget(gv, gvk, gvc, w2[end]) == 1) { *rs = b; *re = end; return 1; }
        if (dg_gvget(gv, gvk, gvc, w2[end]) == 2 && !strcmp(w2[b], "*"))
            { *rs = b; *re = end; return 1; }   /* `* p` 一元解引用 */
        return 0;
    }
    return 0;
}

/* 通用二元运算改写核心 (两遍 omit+重建):
 *   对 w2[0..N) 中"两侧均为 op 类型变量"的二元运算改写成
 *   operator_<wrd>_<opbase>( l , r ); 结果写入 out (调用方已建空串).
 *   arith=1 含 + - * / % (语句级), arith=0 仅比较 == != < <= > >= (泛型体).
 *   操作数 span 由 dg_gbody_op_left/right 依 gv/gvk 分类表识别.
 *   返回改写点数; 0 = 无改写. omit 按 N 动态分配 (N 可达 DG_TOKENCAP_N,
 *   栈上固定 512 会越界 —— 泛型大函数体 (set/at 数百词) 必触). */
static int dg_binop_rewrite(CString *out, char (*w2)[128], int N,
                            char gv[][40], const int *gvk, int gvc,
                            const char *opbase, int arith)
{
    char lbt[64][256], rbt[64][256], opw[64][8];
    int opat[64], onp = 0, q;
    char *omit = tcc_malloc(N ? N : 1);
    memset(omit, 0, N ? N : 1);
    for (q = 0; q < N; q++) {
        const char *wc = w2[q], *wrd = NULL;
        int cmp = 0;
        int lop = 0, rop = 0, ls = q - 1, le = q - 1, rs = -1, re = -1, jj;
        if (!strcmp(wc, "+")) wrd = "add";
        else if (!strcmp(wc, "-")) wrd = "sub";
        else if (!strcmp(wc, "*")) wrd = "mul";
        else if (!strcmp(wc, "/")) wrd = "div";
        else if (!strcmp(wc, "%")) wrd = "mod";
        else if (!strcmp(wc, "==")) { wrd = "eq"; cmp = 1; }
        else if (!strcmp(wc, "!=")) { wrd = "ne"; cmp = 1; }
        else if (!strcmp(wc, "<"))  { wrd = "lt"; cmp = 1; }
        else if (!strcmp(wc, "<=")) { wrd = "le"; cmp = 1; }
        else if (!strcmp(wc, ">"))  { wrd = "gt"; cmp = 1; }
        else if (!strcmp(wc, ">=")) { wrd = "ge"; cmp = 1; }
        else continue;
        if (!arith && !cmp)
            continue;               /* 仅比较: 跳过算术运算符 */
        if (dg_gbody_op_left(w2, q, gv, gvk, gvc, &ls, &le))
            lop = 1;
        if (dg_gbody_op_right(w2, N, q, gv, gvk, gvc, &rs, &re))
            rop = 1;
        if (lop && rop && onp < 64) {
            dg_join_words(w2, ls, le, lbt[onp], sizeof lbt[onp]);
            dg_join_words(w2, rs, re, rbt[onp], sizeof rbt[onp]);
            snprintf(opw[onp], sizeof opw[onp], "%s", wrd);
            opat[onp] = q;
            for (jj = ls; jj <= le; jj++) omit[jj] = 1;
            for (jj = rs; jj <= re; jj++) omit[jj] = 1;
            onp++;
        }
    }
    for (q = 0; q < N; q++) {
        int p, isop = -1;
        if (omit[q])
            continue;
        for (p = 0; p < onp; p++)
            if (opat[p] == q) { isop = p; break; }
        if (isop >= 0) {
            if (out->size && out->data[out->size - 1] != ' ') cstr_ccat(out, ' ');
            cstr_cat(out, "operator_", 9);
            cstr_cat(out, opw[isop], strlen(opw[isop]));
            cstr_ccat(out, '_');
            cstr_cat(out, opbase, strlen(opbase));
            cstr_cat(out, "( ", 2);
            cstr_cat(out, lbt[isop], strlen(lbt[isop]));
            cstr_cat(out, " , ", 3);
            cstr_cat(out, rbt[isop], strlen(rbt[isop]));
            cstr_cat(out, " )", 2);
        } else {
            if (out->size && out->data[out->size - 1] != ' ') cstr_ccat(out, ' ');
            cstr_cat(out, w2[q], strlen(w2[q]));
        }
    }
    tcc_free(omit);
    return onp;
}

/* 在 operator 定义行收集操作数基础类型名 (供泛型体改写判断) */
static void dg_optyp_collect_line(void)
{
    int i;
    for (i = 0; i < dg_n; i++) {
        const char *tx = dg_txt[i] ? dg_txt[i] : "";
        int isop = (dg_buf[i] == TOK_OPERATOR)
                || (isid((unsigned char)tx[0]) && !strncmp(tx, "operator_", 9));
        int j, d = 0;
        if (!isop)
            continue;
        for (j = i + 1; j < dg_n; j++) {
            if (is_space(dg_buf[j]))
                continue;
            if (dg_tokchar(j) == '(') { d = 1; continue; }
            if (d > 0) {
                const char *tj;
                if (dg_tokchar(j) == ')')
                    break;
                if (!isid((unsigned char)(dg_tokchar(j) ? dg_tokchar(j) : 0)))
                    continue;
                tj = dg_txt[j] ? dg_txt[j] : "";
                if (!strcmp(tj, "struct") || !strcmp(tj, "union"))
                    continue;                 /* 跳到类型名本体 */
                dg_optyp_add(tj);
                break;
            }
        }
    }
}

/* 泛型体比较/判等改写: 输出到 out. fp/fbody 为已展开文本; opbase 空则透传. */
static void dg_gbody_oprewrite(CString *out, const char *fp_txt,
                               const char *body_txt, const char *opbase)
{
    char w[DG_TOKENCAP_N][128], w2[DG_TOKENCAP_N][128];
    char gv[288][40]; int gvk[288]; int gvc = 0;
    int n, N, k;

    if (!opbase || !*opbase || !dg_optyp_has(opbase)) {
        cstr_cat(out, body_txt ? body_txt : "", body_txt ? strlen(body_txt) : 0);
        return;
    }
    n = dg_splitw(fp_txt ? fp_txt : "", w, DG_TOKENCAP_N);
    N = dg_splitw(body_txt ? body_txt : "", w2, DG_TOKENCAP_N);

#define GV_SET(nm,kd) do { int q,fo=0; for(q=0;q<gvc;q++) if(!strcmp(gv[q],nm)){gvk[q]=kd;fo=1;break;} if(!fo&&gvc<288){snprintf(gv[gvc],40,"%s",nm);gvk[gvc]=kd;gvc++;} } while(0)
    /* 形参: 每段尾标识 = 参数名; 段内类型基名==opbase → OP 值 */
    for (k = 0; k < n; k++) {
        int j, seg_end = -1, name = -1, tb = -1;
        if (!isid((unsigned char)w[k][0]) || w[k][0] == '*')
            continue;
        for (j = k; j < n; j++)
            if (!strcmp(w[j], ",") || !strcmp(w[j], ")")) { seg_end = j; break; }
        for (j = (seg_end < 0 ? n - 1 : seg_end - 1); j >= k; j--)
            if (isid((unsigned char)w[j][0]) && strcmp(w[j], "struct")
                && strcmp(w[j], "const") && strcmp(w[j], "unsigned")) { name = j; break; }
        if (name >= k) {
            for (j = name - 1; j >= 0 && j >= k; j--) {
                if (!strcmp(w[j], ","))
                    break;
                if (isid((unsigned char)w[j][0])) { tb = j; break; }
            }
            if (tb >= 0 && !strcmp(dg_typbase(w[tb]), opbase)) {
                /* 类型到名称间有 '*' → OP 指针(解用下标/一元解引用); 否则 OP 值 */
                int star = 0, q;
                for (q = tb + 1; q < name; q++)
                    if (!strcmp(w[q], "*")) { star = 1; break; }
                GV_SET(w[name], star ? 2 : 1);
            }
        }
        k = (seg_end < 0) ? n : seg_end;
    }
    /* 局部声明: `opbase [*] v` 或 `struct opbase [*] v` → 指针(2)/值(1) */
    for (k = 0; k < N; k++) {
        int typeidx = -1, j, star = 0, v = -1;
        if (isid((unsigned char)w2[k][0]) && !strcmp(dg_typbase(w2[k]), opbase))
            typeidx = k;
        else if (!strcmp(w2[k], "struct") && k + 1 < N
                 && isid((unsigned char)w2[k + 1][0])
                 && !strcmp(dg_typbase(w2[k + 1]), opbase))
            typeidx = k + 1;
        if (typeidx < 0)
            continue;
        j = typeidx + 1;
        while (j < N && !strcmp(w2[j], "*")) { star = 1; j++; }
        while (j < N && !isid((unsigned char)w2[j][0])) j++;
        if (j < N && strcmp(w2[j], "struct") && strcmp(w2[j], "const"))
            { v = j; GV_SET(w2[v], star ? 2 : 1); }
    }
#undef GV_SET

    /* 泛型体内比较/判等改写: 由共用核心 dg_binop_rewrite 完成 (arith=0 仅比较运算);
     * 无改写点时核心仍把全部 token 原样重建进 out, 等价旧 P2a/P2b 透传. */
    dg_binop_rewrite(out, w2, N, gv, gvk, gvc, opbase, 0);
}

static void dg_model_f_register(TCCState *s1, const DgModelDef *m, char av[][64])
{
    char sn[384];
    DgModelFDef *fp;
    CString ret, fp_, body, def;
    if (!dg_fout_td_init) {
        cstr_new(&dg_fout_td);
        dg_fout_td_init = 1;
    }
    dg_model_synth(m, av, sn, sizeof sn);
    for (fp = dg_fdefs; fp; fp = fp->next)
        if (!strcmp(fp->synth, sn))
            return;                        /* 已登记 */
    fp = tcc_malloc(sizeof *fp);
    memset(fp, 0, sizeof *fp);
    fp->synth = tcc_strdup(sn);
    /* 展开: tag 形式引用 + 共享 dg_fout_td 累积 typedef.
     * 注意 expand_src 输出 CString 不带 NUL, 全程用 .size 而非 strlen. */
    cstr_new(&ret); cstr_new(&fp_); cstr_new(&body); cstr_new(&def);
    /* 登记当前函数泛型的抽象名/合成实例名, 供泛型体自递归裸调用绑定 */
    dg_fdef_name = m->ntxt;
    snprintf(dg_fdef_synth, sizeof dg_fdef_synth, "%s", sn);
    dg_model_expand_src(s1, m, av, m->bret, m->eret, &ret);
    dg_model_expand_src(s1, m, av, m->bfp, m->efp, &fp_);
    dg_model_expand_src(s1, m, av, m->bbody, m->ebody, &body);
    dg_fdef_name = NULL;
    dg_fdef_synth[0] = 0;
    /* P1: 泛型体内 operator 改写 — 类型实参 T(av[0]) 为 op 类型时, 比较/判等
     * 调 operator_X; 否则(int 等)原样透传. 注意复制 opbase 到局部, 因
     * dg_typbase 返回 static 缓冲, 持久引用会被后续调用覆盖. */
    if (m->nparams > 0 && m->pk[0] == 't' && dg_optyp_has(av[0])) {
        char ob[40], *bdn = NULL, *fpn = NULL;
        CString b2;
        snprintf(ob, sizeof ob, "%s", dg_typbase(av[0]));
        /* expand_src 输出 CString 无 NUL, 而 splitw 依赖 \0 扫描 → 必须补 NUL
         * 副本, 否则越界读其后的函数内容(乱码). */
        bdn = tcc_malloc(body.size + 1);
        memcpy(bdn, body.data, body.size); bdn[body.size] = 0;
        fpn = tcc_malloc(fp_.size + 1);
        memcpy(fpn, fp_.data, fp_.size); fpn[fp_.size] = 0;
        cstr_new(&b2);
        dg_gbody_oprewrite(&b2, fpn ? fpn : "", bdn ? bdn : "", ob);
        tcc_free(bdn); tcc_free(fpn);
        cstr_free(&body);
        body = b2;   /* 体转交改写结果 */
    }
    {
        const char *R = ret.size ? ret.data : "int";
        const char *B = body.size ? body.data : ";";
        int Rlen = ret.size ? (int)ret.size : 3;
        int Blen = body.size ? (int)body.size : 1;
        cstr_cat(&def, "static ", 7);
        cstr_cat(&def, R, Rlen);
        cstr_ccat(&def, ' ');
        cstr_cat(&def, sn, strlen(sn));
        if (fp_.size && fp_.data)
            cstr_cat(&def, fp_.data, (int)fp_.size);
        cstr_cat(&def, "\n{\n", 3);
        cstr_cat(&def, B, Blen);
        cstr_cat(&def, "\n}\n", 3);
        cstr_ccat(&def, 0);
        fp->def = tcc_strdup(def.data);
    }
    cstr_free(&ret); cstr_free(&fp_); cstr_free(&body); cstr_free(&def);
    fp->next = dg_fdefs;
    dg_fdefs = fp;
}

/* EOF 收尾拆两段: 结构/nested 合成 typedef 与函数泛型定义, 由 --emit-c 回放按
 * 位置分别前置 (typedef 提到 header 展开之后供顶层用户代码先见; 函数泛型定义
 * 再插到 int main 前充当原型). */
static void dg_fdefs_typedefs(TCCState *s1)
{
    if (dg_fout_td_init && dg_fout_td.size && dg_fout_td.data) {
        fwrite(dg_fout_td.data, 1, dg_fout_td.size, s1->ppfp);
        fputc('\n', s1->ppfp);
    }
}
static void dg_fdefs_funcs(TCCState *s1)
{
    DgModelFDef *fp;
    for (fp = dg_fdefs; fp; fp = fp->next)
        if (fp->def)
            fputs(fp->def, s1->ppfp);
}

/* 从 token 缓冲解析一个 model 实例 (i 指向模板名, 其后应为 '('), 返回实参.
 * 复用 dg_model_av_from_words: 将 [i,dg_n) 展平为单词数组后委托解析, 结束下标
 * 经 wp 回映为 token 下标 ('(' 前/')' 后的空白 token 一并保留, 字节精确).
 * w/wp 走堆分配: 栈上 256KB 数组在嵌套实例路径 (本函数 → from_words → emit
 * → expand_src 512KB 帧) 会叠加击穿宿主进程栈 (t062 嵌套 STL_Pair 崩溃根因). */
static int dg_model_av_from_tokens(TCCState *s1, int i,
                                   const DgModelDef *m,
                                   char av[][64], int *pend)
{
    char (*w)[128] = tcc_malloc((size_t)DG_TOKENCAP_N * 128);
    int *wp = tcc_malloc((size_t)DG_TOKENCAP_N * sizeof(int));
    int n = 0, j, end, r = 0;
    for (j = i; j < dg_n && n < DG_TOKENCAP_N; j++) {
        if (is_space(dg_buf[j]))
            continue;
        snprintf(w[n], 128, "%s", dg_txt[j] ? dg_txt[j] : "");
        wp[n] = j;
        n++;
    }
    if (n >= 2 && !strcmp(w[1], "(") &&     /* 模板名后须紧跟 '(' */
        !(n >= 3 && !strcmp(w[2], ")")) &&  /* 空实参 Model() */
        dg_model_av_from_words(s1, w, n, 0, m, av, &end)) {
        *pend = wp[end - 1] + 1;            /* ')' 紧后 token, 保留中间空白 */
        r = 1;
    }
    tcc_free(wp);
    tcc_free(w);
    return r;
}

/* i 处为 model 实例化点 (模板名紧接 '('): 解析实参并落地其定义.
 * 函数泛型 (kind=='F') 在 fok=1 时走 dg_model_f_register (EOF 统一发射),
 * fok=0 时跳过; 其余实例走 dg_model_emit 立即落地 typedef. 命中填 *sn
 * 合成名并返回结束 token 下标 (调用方 i = ret-1 续扫), 非实例化返回 -1. */
static int dg_model_inst_end(TCCState *s1, int i, int to, int fok,
                             char *sn, int snsz)
{
    DgModelDef *mm;
    char av[DG_MODEL_MAXP][64];
    int nx, end;
    if (i >= to || dg_buf[i] < TOK_IDENT)
        return -1;
    mm = dg_model_find(dg_txt[i] ? dg_txt[i] : "");
    if (!mm)
        return -1;
    nx = dg_next_ns(i);
    if (nx < 0 || nx >= to || nx >= dg_n || dg_tokchar(nx) != '(')
        return -1;
    if (!dg_model_av_from_tokens(s1, i, mm, av, &end))
        return -1;
    if (mm->kind == 'F') {
        if (!fok)
            return -1;
        dg_model_f_register(s1, mm, av);
    } else {
        dg_model_emit(s1, mm, av);
    }
    if (sn && snsz > 0)
        dg_model_synth(mm, av, sn, snsz);
    return end;
}

/* 预扫描 [from,to) 内全部 model 实例化点, 先把合成 typedef 落地到 ppfp.
 * 保证 `Box(double)` 即使在表达式内 (sizeof/实参) 也能先以语句级 typedef 声明,
 * 后续 token 处仅替换为合成类型名, 避免在表达式中内联 typedef (语法错误). */
static void dg_emit_verbatim_pre_models(TCCState *s1, int from, int to)
{
    int i;
    for (i = from; i < to; i++) {
        int e;
        if (is_space(dg_buf[i]))
            continue;
        e = dg_model_inst_end(s1, i, to, 0, NULL, 0);
        if (e >= 0) {          /* fok=0: 函数泛型跳过, 仅落地非 F 实例 typedef */
            i = e - 1;
            continue;
        }
    }
}

static void dg_emit_verbatim(TCCState *s1, int from, int to)
{
    int i, last = 0;
    /* 先把本单元内 model 实例的合成 typedef 落地到语句级, 再逐 token 替换名字 */
    dg_emit_verbatim_pre_models(s1, from, to);
    for (i = from; i < to; i++) {
        int t = dg_buf[i];
        if (is_space(t)) {          /* 空格 token: 快照精确重放 */
            fputs(dg_txt[i] ? dg_txt[i] : " ", s1->ppfp);
            continue;
        }
        if (last && pp_need_space(last, t))
            fputc(' ', s1->ppfp);
        /* model 实例化: 模板名紧接 '(' -> 落地定义并改写为合成名.
         * 函数泛型 (`Name(targs)(args)`) 只登记并改写名字, `(args)` 原样续写;
         * 其 typedef/定义在 EOF 统一发射. */
        if (t >= TOK_IDENT) {
            char sn[384];
            int e = dg_model_inst_end(s1, i, to, 1, sn, sizeof sn);
            if (e >= 0) {
                fputs(sn, s1->ppfp);
                last = t;
                i = e - 1;      /* 跳过模板名与实参 */
                continue;
            }
        }
        if (t == TOK_builtin_reflect ||
            (dg_txt[i] && !strcmp(dg_txt[i], "__builtin_reflect"))) {
            /* __builtin_reflect(struct X / union U / enum E) -> (&<T>_refl)
             * 由 dg_reflect_emit 生成对应静态反射表, 供顶层用户代码先见. */
            int j, kw = 0, close = -1, m, c;
            char tag[128] = { 0 };
            j = i + 1;
            while (j < to && is_space(dg_buf[j])) j++;
            if (j < to && dg_tokchar(j) == '(' && (j + 1) < to) {
                const char *wkw;
                int k = j + 1;
                while (k < to && is_space(dg_buf[k])) k++;
                wkw = (k < to && dg_txt[k]) ? dg_txt[k] : "";
                if (!strcmp(wkw, "struct")) kw = 'S';
                else if (!strcmp(wkw, "union")) kw = 'U';
                else if (!strcmp(wkw, "enum")) kw = 'E';
            }
            if (kw) {
                m = j + 1;
                while (m < to && is_space(dg_buf[m])) m++;
                if (m < to) {   /* 跳过 struct/union/enum 关键字文本(其在 desugar 是普通标识符) */
                    const char *tm = dg_txt[m] ? dg_txt[m] : "";
                    if (!strcmp(tm, "struct") || !strcmp(tm, "union") || !strcmp(tm, "enum"))
                        m++;
                }
                while (m < to && is_space(dg_buf[m])) m++;
                if (m < to && dg_txt[m])
                    snprintf(tag, sizeof tag, "%s", dg_txt[m]);
                c = m + 1;
                while (c < to && is_space(dg_buf[c])) c++;
                if (c < to && dg_tokchar(c) == ')')
                    close = c;
            }
            if (tag[0] && close >= 0) {
                dg_used_reflect = 1;
                fputs("(&", s1->ppfp);
                fputs(tag, s1->ppfp);
                fputs("_refl)", s1->ppfp);
                last = t;
                i = close;      /* 跳过 (struct X) */
                continue;
            }
            /* 未知用法: 原样透传 */
            fputs(dg_txt[i] ? dg_txt[i] : "", s1->ppfp);
            last = t;
            continue;
        }
        if (t == TOK_OPERATOR) {
            int opid = dg_opat(i);     /* 顺带注册 op */
            if (opid) {
                fputs(dg_op_nm_txt(opid, dg_op_def_typ(i)), s1->ppfp);
                last = t;
                i = dg_next_ns(i);     /* 跳过紧跟的运算符字符 token */
                continue;
            }
        }
        fputs(dg_txt[i] ? dg_txt[i] : "", s1->ppfp);
        last = t;
    }
}

/* 尝试改写表达式区域 [from,to): 命中则发射改写后的标准 C 文本并返回 1;
 * 未命中/非简单/未启用则返回 0 (不发射, 调用方回退 verbatim). */
static int dg_region_rewrite(TCCState *s1, int from, int to)
{
    int ypos, root, z;
    if (!dg_active || from < 0 || from >= to)
        return 0;
    /* 含 __builtin_reflect: 交由 verbatim (dg_emit_verbatim 有专门改写分支),
     * 本 AST 路径不识别该 builtin. */
    for (z = from; z < to; z++)
        if (!is_space(dg_buf[z]) &&
            (dg_buf[z] == TOK_builtin_reflect ||
             (dg_txt[z] && !strcmp(dg_txt[z], "__builtin_reflect"))))
            return 0;
    dg_nndo = 0;
    ypos = from;
    root = dg_expr(&ypos, 1);
    if (root >= 0 && ypos >= to && ypos > from
        && dg_check_simple(from, ypos) && dg_has_rewrite(root)) {
        CString out;
        cstr_new(&out);
        dg_pnode(&out, root);
        if (out.size) {
            fwrite(out.data, 1, out.size, s1->ppfp);
            cstr_free(&out);
            return 1;
        }
        cstr_free(&out);
    }
    return 0;
}

/* 行内任意位置出现 defer 则需走 defer 专属发射 (支持同行多 defer 与 `{`/`}`). */
static int dg_has_defer(void)
{
    int i;
    for (i = 0; i < dg_n; i++)
        if (!is_space(dg_buf[i]) && dg_buf[i] == TOK_DEFER)
            return 1;
    return 0;
}

/* 提取 index idefer 处的 `defer <call text>` 调用文本 (defer 后至顶层 `;`,
 * 紧凑拼接, 含 operator 名改写), 入栈到深度 depth; 返回调用语句 `;` 的 token
 * 下标 (供跳过), 无有效文本/无 `;` 时返回 idefer (不消费). */
static int dg_defer_pick(TCCState *s1, int idefer, int depth)
{
    int i, start, end, last = 0;
    CString cs;
    (void)s1;
    start = -1;
    for (i = idefer + 1; i < dg_n; i++)
        if (!is_space(dg_buf[i])) { start = i; break; }
    if (start < 0)
        return idefer;
    end = -1;
    for (i = start; i < dg_n; i++) {
        int ch = dg_tokchar(i);
        if (ch == '(' && dg_pair[i] >= 0) {
            i = dg_pair[i];
        } else if (ch == ';') {
            end = i;
            break;
        }
    }
    if (end < 0)
        return idefer;
    if (depth < 0)
        depth = 0;
    if (depth >= DG_DEFER_MAXDEP || dg_defer_n[depth] >= DG_DEFER_MAXN)
        return end;
    cstr_new(&cs);
    for (i = start; i < end; i++) {
        int t = dg_buf[i];
        if (is_space(t))
            continue;
        if (pp_need_space(last, t))
            cstr_ccat(&cs, ' ');
        if (t == TOK_OPERATOR) {
            int opid = dg_opat(i);
            if (opid) {
                cstr_cat(&cs, dg_op_name[opid], strlen(dg_op_name[opid]));
                last = t;
                i = dg_next_ns(i);
                continue;
            }
        }
        cstr_cat(&cs, dg_txt[i] ? dg_txt[i] : "", strlen(dg_txt[i] ? dg_txt[i] : ""));
        last = t;
    }
    cstr_ccat(&cs, 0);
    dg_defer[depth][dg_defer_n[depth]++] = tcc_strdup(cs.data);
    cstr_free(&cs);
    return end;
}

/* 逆序发射第 dep 层的全部 defer 调用 (闭块处调用, 发射并释放) */
static void dg_emit_defer_level(TCCState *s1, int dep)
{
    int k;
    if (dep < 0 || dep >= DG_DEFER_MAXDEP)
        return;
    for (k = dg_defer_n[dep] - 1; k >= 0; k--) {
        if (dg_defer[dep][k]) {
            /* 缩进以示在块结束处逆序调用 */
            fputs("    ", s1->ppfp);
            fputs(dg_defer[dep][k], s1->ppfp);
            fputs(";\n", s1->ppfp);
            tcc_free(dg_defer[dep][k]);
            dg_defer[dep][k] = NULL;
        }
    }
    dg_defer_n[dep] = 0;
}

/* 早退 (return) 触发: 从当前深度 dg_dep 到函数体层 1 逆序发射各层 defer.
 * 顺序 = 先内层后外层 (更高深度先), 每层内 LIFO. 关键: 只发射不释放 ——
 * return 是终端的, 每次调用只执行其中一条 return, 因此每个 return 点都须把
 * 尚在作用域的 defer 全部内联落地; 若此处释放, 其它分支的 return 就丢了它们
 * (如 f(0) 的 return 9)。闭块 `}` 处的 dg_emit_defer_level 仍会释放, 形成
 * return 与闭块两条收口路径各司其职, 不重复执行 (return 后是死代码). */
static void dg_emit_defer_all(TCCState *s1, int topdep)
{
    int d, k;
    if (topdep < 1)
        topdep = 1;
    for (d = topdep; d >= 1; d--) {
        for (k = dg_defer_n[d] - 1; k >= 0; k--) {
            if (dg_defer[d][k]) {
                fputs("    ", s1->ppfp);
                fputs(dg_defer[d][k], s1->ppfp);
                fputs(";\n", s1->ppfp);
            }
        }
    }
}

/* defer 混合行专属发射: 行内任意位置含 defer 时, 收集全部 defer (按行内块深度
 * 入栈), 落地其余 token, 行内闭合的 `}` 立即逆序重放其层 defer. 支持
 * `{ defer a(); defer b(); ... }` 同行多 defer 与 defer 后接普通语句; 遇 `return`
 * 早退时先把当前存活 defer (cur..1 层) 内联发射到 return 前, 并用 `{ }` 包住
 * return 语句, 保证条件 return (如 `if (c) return 2;`) 只在真分支发射、不破坏
 * 后续控制流 (t029/ret_test 的顶层 return 也能在返回前执行 rec(20)). */
static void dg_flush_defer_line(TCCState *s1)
{
    int i, cur = (dg_dep < 0) ? 0 : dg_dep, prev = 0;
    for (i = 0; i < dg_n; i++) {
        int t = dg_buf[i];
        if (is_space(t)) {
            fputs(dg_txt[i] ? dg_txt[i] : " ", s1->ppfp);
            continue;
        }
        if (t == TOK_DEFER) {
            int e = dg_defer_pick(s1, i, cur);
            i = e;              /* 跳过整个 defer 语句 (其 `;` 由 e 指向) */
            prev = t;
            continue;
        }
        if (t == TOK_RETURN) {
            /* return 早退: 把已进入作用域的 defer 先落地, 再包块返回, 避免跳走
             * 后续 `}` 重放而丢 defer 或使条件 return 破坏控制流. */
            int depth = 0, j;
            fputs("{\n", s1->ppfp);
            dg_emit_defer_all(s1, cur);
            fputs("    ", s1->ppfp);
            fputs(dg_txt[i] ? dg_txt[i] : "return", s1->ppfp);
            for (j = i + 1; j < dg_n; j++) {
                int u = dg_buf[j];
                char c = dg_tokchar(j);
                if (is_space(u)) {
                    fputs(dg_txt[j] ? dg_txt[j] : " ", s1->ppfp);
                    continue;
                }
                if (c == '(' || c == '[') depth++;
                else if ((c == ')' || c == ']') && depth > 0) depth--;
                if (c == ';' && depth == 0) { i = j; break; }   /* 停于 ';' 前 */
                if (prev && pp_need_space(prev, u))
                    fputc(' ', s1->ppfp);
                fputs(dg_txt[j] ? dg_txt[j] : "", s1->ppfp);
                prev = u;
            }
            fputs(";\n}\n", s1->ppfp);
            prev = ';';
            continue;
        }
        {
            char ch = dg_tokchar(i);
            if (ch == '{' && prev != '=' && prev != ',') {
                if (cur < DG_DEFER_MAXDEP - 1)
                    cur++;
            } else if (ch == '}') {
                if (cur > 0) {
                    dg_emit_defer_level(s1, cur);
                    cur--;
                }
            }
        }
        if (prev && pp_need_space(prev, t))
            fputc(' ', s1->ppfp);
        fputs(dg_txt[i] ? dg_txt[i] : "", s1->ppfp);
        prev = t;
    }
    dg_dep = cur;
}

/* model 定义收集: 把本行非空 token 追加进 dgm token 序列, 跟踪花括号深度;
 * 外层 `}` 闭合且可选 `;` 已吞时调用 dg_model_finish 并返回 1 (定义完成). */
static int dg_model_collect(TCCState *s1)
{
    (void)s1;
    for (int i = 0; i < dg_n; i++) {
        int t = dg_buf[i];
        const char *s;
        if (is_space(t))
            continue;
        s = dg_txt[i] ? dg_txt[i] : "";
        if (!strcmp(s, "{")) {
            dgm_add(t, s);
            dg_mbr++;
            if (dg_mbase < 0)
                dg_mbase = dg_mbr;   /* 首 "{" = 定义体开括号, 记顶层深度基准 */
        } else if (!strcmp(s, "}")) {
            dgm_add(t, s);
            dg_mbr--;
            /* 仅当回到体开括号前的包围深度时才视为定义闭合 —— 用相对基准而非
             * 绝对值 0, 防止宏展开(struct 定义 / do{...}while 的花括号)把计数
             * 提前压到 0 而在真正函数结尾前截断(set 缺陷). */
            if (dg_mbase > 0 && dg_mbr < dg_mbase) {
                dg_msemi = 1;
                return 1;
            }
        } else if (!strcmp(s, ";")) {
            dgm_add(t, s);
            if (dg_mbase <= 0 && dg_mbr <= 0) {
                dg_msemi = 1;
                return 1;
            }
        } else {
            dgm_add(t, s);
            if (dg_mbody == 0)
                dg_mbody = 1;   /* 首次非空 token 过后视为进入定义体 */
        }
    }
    /* 本行尚未闭合 (跨行 model 定义), 继续收集 */
    return dg_mbase > 0 && dg_mbr < dg_mbase && dg_msemi;
}

/* 是否为一段 model 定义 (首非空 token 为 TOK_MODEL)? */
static int dg_is_model_line(void)
{
    int i;
    for (i = 0; i < dg_n; i++)
        if (!is_space(dg_buf[i]))
            return dg_buf[i] == TOK_MODEL;
    return 0;
}

/* 语句级二进制运算改写: 对一整行中"两侧均为 operator 类型变量"的二元算术/比较
 * (+ - * / % == != < <= > >=) 改写成 operator_<wrd>_<opbase>(l, r). 周边 `!`/`(`
 * /`if`/`(`) 等保持原样 verbatim —— 不解析整表达式树 (DGNode 对未注册一元 ! 会误拼
 * 出口). 安全前提: 仅当两侧操作数都是登记过的 op 类型变量才改写, 普通 int/指针
 * 比较不受影响. 命中并整行重建返回 1 (调用方 emit 收尾), 否则返回 0. */
static int dg_line_rewrite(TCCState *s1)
{
    char w2[512][128]; int widx[512]; int N = 0, k, i;
    char gv[288][40]; int gvk[288]; int gvc = 0;
    char opbase[40];
    CString out;

    if (!dg_active)
        return 0;
    for (i = 0; i < dg_n; i++) {
        if (is_space(dg_buf[i]))
            continue;
        if (N >= 512)
            break;
        snprintf(w2[N], sizeof w2[N], "%s", dg_txt[i] ? dg_txt[i] : "");
        widx[N] = i;
        N++;
    }
    if (N < 3)
        return 0;
    opbase[0] = 0;
    /* 收集当前行内 op 类型变量: 名 -> gv, kind = 指针?2:1; 记录首个 opbase */
    for (k = 0; k < N; k++) {
        const char *w = w2[k];
        int tag, ptr, qo, fo = 0;
        if (!isid((unsigned char)w[0]))
            continue;
        tag = dg_var_of(dg_buf[widx[k]]);
        if (!tag)
            continue;
        {
            const char *nm = get_tok_str(tag, NULL);
            if (!nm || !*nm || !dg_optyp_has(nm))
                continue;
            ptr = dg_var_isptr(dg_buf[widx[k]]);
            for (qo = 0; qo < gvc; qo++)
                if (!strcmp(gv[qo], w)) { gvk[qo] = ptr ? 2 : 1; fo = 1; break; }
            if (!fo && gvc < 288) {
                snprintf(gv[gvc], sizeof gv[gvc], "%s", w);
                gvk[gvc] = ptr ? 2 : 1;
                gvc++;
            }
            if (!opbase[0])
                snprintf(opbase, sizeof opbase, "%s", nm);
        }
    }
    if (!opbase[0])
        return 0;                       /* 本行无 op 类型变量 */
    /* 语句级算术+比较改写: 由共用核心 dg_binop_rewrite 完成 (arith=1 全运算符).
     * 无改写点时返回 0, 交由上层 verbatim/其它 pass 处理 (与旧行为一致). */
    cstr_new(&out);
    if (dg_binop_rewrite(&out, w2, N, gv, gvk, gvc, opbase, 1)) {
        cstr_ccat(&out, 0);
        fputs(out.data, s1->ppfp);
        cstr_free(&out);
        return 1;
    }
    cstr_free(&out);
    return 0;
}

/* 泛型对象方法糖改写: `recv -> mname ( targs ) ( args )` → `mname ( targs ) ( & recv , args )`.
 * 判据: 标识符接着 TOK_ARROW, 其后是另一标识符紧跟 **两个连续括号组** —— 这是
 * model 泛型方法调用的签名 `mname(targs)(args)`, 不会与普通 `p->field(x)`(单括号)
 * 或成员函数指针(单括号)混淆. 重构本行 token 数组并返回 1(有改写). */
static int dg_sugar_rewrite(TCCState *s1)
{
    (void)s1;
    for (;;) {
        int hit = 0, i;
        for (i = 0; i < dg_n; i++) {
            int recv = -1, mnam = -1, targ_o = -1, targ_c = -1, arg_o = -1, j, depth;
            int k, d;
            int *nbuf; char **ntxt; int nb, nnew;
            if (is_space(dg_buf[i]) || dg_buf[i] != TOK_ARROW)
                continue;
            for (j = i - 1; j >= 0; j--) if (!is_space(dg_buf[j])) { recv = j; break; }
            if (recv < 0 || dg_buf[recv] < TOK_IDENT) continue;
            /* vptr / 成员间接调用 `obj.fn->m(...)`、`a->f->g(...)` 不是泛型方法糖:
             * arrow 左侧若是成员访问(前置 `.` / `->`)则跳过, 保留原样(标准 C 合法). */
            {
                int pj;
                for (pj = recv - 1; pj >= 0; pj--)
                    if (!is_space(dg_buf[pj])) break;
                if (pj >= 0 && (dg_tokchar(pj) == '.' || dg_buf[pj] == TOK_ARROW))
                    continue;
            }
            for (j = i + 1; j < dg_n; j++) if (!is_space(dg_buf[j])) { mnam = j; break; }
            if (mnam < 0 || dg_buf[mnam] < TOK_IDENT) continue;
            targ_o = mnam + 1;
            while (targ_o < dg_n && is_space(dg_buf[targ_o])) targ_o++;
            if (targ_o >= dg_n || dg_tokchar(targ_o) != '(') continue;
            depth = 0;
            for (j = targ_o; j < dg_n; j++) {
                char ch = dg_tokchar(j);
                if (ch == '(') depth++;
                else if (ch == ')' && --depth == 0) { targ_c = j; break; }
            }
            if (targ_c < 0) continue;
            arg_o = targ_c + 1;
            while (arg_o < dg_n && is_space(dg_buf[arg_o])) arg_o++;
            /* 泛型形式: recv->mname(targs)(args) —— 第二段 '(' 存在;
             * 非泛型形式: recv->mname(args) —— 无第二段, targ_o..targ_c 即实参组. */
            {
                int generic = (arg_o < dg_n && dg_tokchar(arg_o) == '(');
                int amp = dg_var_isptr(dg_buf[recv]) ? 0 : 1;  /* 指针接收器不再取址 */
                /* 实参是否非空: 泛型扫 [arg_o+1, 收尾')'), 非泛型扫 [targ_o+1, targ_c] */
                int ahs = 0, al, ac;
                if (generic) {
                    al = arg_o + 1; d = 0;
                    for (k = al; k < dg_n; k++) {
                        char c;
                        if (is_space(dg_buf[k])) continue;
                        c = dg_tokchar(k);
                        if (c == '(') d++;
                        else if (c == ')' && d > 0) d--;
                        else if (c == ')' && d == 0) break;
                        else ahs = 1;
                    }
                    ac = -1;
                } else {
                    for (k = targ_o + 1; k < targ_c; k++)
                        if (!is_space(dg_buf[k])) { ahs = 1; break; }
                    al = 0; /* 未用 */
                    ac = targ_c;
                }
                /* ---- 命中: 重建为 mname (targs) ( [&] recv [, args...] ) (或单括号) ----
                 * 缓冲余量取 dg_n+8 ≥ 实际 nb (泛型至多多插 '(' '&' ',' 3 个). */
                nnew = dg_n + 8;
                nbuf = tcc_malloc(nnew * sizeof(int));
                ntxt = tcc_malloc(nnew * sizeof(char *));
                nb = 0;
#define DG_MOVE(TK) do { nbuf[nb]=dg_buf[TK]; ntxt[nb]=dg_txt[TK]; nb++; } while (0)
#define DG_PUT(CH) do { nbuf[nb]=(CH); ntxt[nb]=(char *)tcc_malloc(2); ntxt[nb][0]=(CH); ntxt[nb][1]=0; nb++; } while (0)
                for (j = 0; j < recv; j++) DG_MOVE(j);        /* 前缀 */
                DG_MOVE(mnam);                                 /* 方法名 */
                if (generic) {
                    for (j = targ_o; j <= targ_c; j++) DG_MOVE(j); /* (targs) */
                    DG_PUT('(');                               /* 调用参数 '(' */
                    if (amp) DG_PUT('&');
                    DG_MOVE(recv);                             /* receiver 注入 */
                    if (ahs) DG_PUT(',');
                    for (j = arg_o + 1; j < dg_n; j++) DG_MOVE(j); /* 原 args + ')' */
                } else {
                    DG_MOVE(targ_o);                           /* '(' 复用 */
                    if (amp) DG_PUT('&');
                    DG_MOVE(recv);                             /* receiver 注入 */
                    if (ahs) DG_PUT(',');
                    for (j = targ_o + 1; j <= targ_c; j++) DG_MOVE(j); /* args... + ')' */
                    for (j = targ_c + 1; j < dg_n; j++) DG_MOVE(j); /* 尾部(; 及后续)原样保留 */
                }
#undef DG_MOVE
#undef DG_PUT
                /* 丢弃旧 lexeme: arrow(i); 泛型还弃 arg_o 的 '(' (旧数组在此刻仍有效) */
                if (dg_txt[i]) { tcc_free(dg_txt[i]); dg_txt[i] = NULL; }
                if (generic && dg_txt[arg_o]) { tcc_free(dg_txt[arg_o]); dg_txt[arg_o] = NULL; }
                for (j = 0; j < nb; j++) { dg_buf[j] = nbuf[j]; dg_txt[j] = ntxt[j]; }
                dg_n = nb;
                tcc_free(nbuf);
                tcc_free(ntxt);
                (void)ac; (void)al;
            }
            hit = 1;
            break;      /* 一次只改一个 -- 跳出内层, 外层 for(;;) 重新扫描新数组 */
        }
        if (!hit)       /* 本轮无其余糖: 全部改写完成 */
            break;
    }
    return 0;
}

/* 构建当前行的配对索引 dg_pair[]. 用一显式栈扫描本行 token, 把 ()[]{} 的
 * 开闭配成对; 跨行的外部括号不参与(本行为局部片段, 交由持久深度栈维护).
 * 所有改写 pass 统一读它, 不再各自对括号做深度计数. */
static void dg_build_pairs(void)
{
    int stk[DG_BUFN], sp = 0, i, ch;
    for (i = 0; i < dg_n; i++)
        dg_pair[i] = -1;
    for (i = 0; i < dg_n; i++) {
        if (is_space(dg_buf[i]))
            continue;
        ch = dg_tokchar(i);
        if (ch == '(' || ch == '[' || ch == '{') {
            stk[sp++] = i;
        } else if (ch == ')' || ch == ']' || ch == '}') {
            int o, och;
            if (sp <= 0)
                continue;
            o = stk[--sp];
            och = dg_tokchar(o);
            if ((och == '(' && ch == ')') || (och == '[' && ch == ']')
                || (och == '{' && ch == '}')) {
                dg_pair[o] = i;
                dg_pair[i] = o;
            }
        }
    }
}

/* ============ reflect 脱糖 (__builtin_reflect -> 静态反射表) ============
 * 收集 struct/union/enum 定义, 在 EOF 前置成 tcc-reflect.h 的 __refl 静态表,
 * __builtin_reflect(T) 改写为 (&<tag>_refl). kind 编码与 tccgen refl_kind 一致:
 * STRUCT=1 UNION=2 PTR=3 INT=4 FLOAT=5 LLONG=6 BYTE=7 BOOL=8 ENUM=9 ARRAY=10
 * VOID=11 SHORT=12 DOUBLE=13 LDOUBLE=14 OTHER=15. */
#define DG_REFLECT_MAXT 64
#define DG_REFLECT_MAXF 128
#define DGR_STRUCT 1
#define DGR_UNION  2
#define DGR_PTR    3
#define DGR_INT    4
#define DGR_FLOAT  5
#define DGR_LLONG  6
#define DGR_BYTE   7
#define DGR_BOOL   8
#define DGR_ENUM   9
#define DGR_ARRAY  10
#define DGR_VOID   11
#define DGR_SHORT  12
#define DGR_DOUBLE 13
#define DGR_LDOUBLE 14
#define DGR_OTHER  15
typedef struct {
    char nm[128];      /* 字段名 */
    int  kind;         /* 字段 kind (数组字段为 ARRAY) */
    char ft[192];      /* _Alignof 基础类型文本: "float"/"struct Vec3"/"int*" */
    char sz[192];      /* size 表达式: "sizeof(float)"/"sizeof(struct Vec3)*4u" */
    char sub[128];     /* sub 子表 tag(嵌套 struct / 数组元素 struct); 空=无 */
    int  isarr, cnt;   /* 数组字段 & 元素个数 */
} DgReflectField;
typedef struct {
    char tag[128]; char kw;    /* kw: 'S'/'U'/'E' */
    DgReflectField f[DG_REFLECT_MAXF]; int nf;
    int emitted;
} DgReflect;
static DgReflect dg_refl[DG_REFLECT_MAXT];

static DgReflect *dg_reflect_find(const char *tag, char kw)
{
    int i;
    for (i = 0; i < dg_refln; i++)
        if (dg_refl[i].kw == kw && !strcmp(dg_refl[i].tag, tag))
            return &dg_refl[i];
    return NULL;
}

/* 类型关键字判断 (struct 定义里区分类型与声明符名) */
static int dg_refl_istypeword(const char *tx)
{
    static const char *w[] = {
        "struct","union","enum","int","float","double","short","long","char",
        "signed","unsigned","void","_Bool","const","volatile","static","extern",
        "typedef","register","inline","restrict","_Atomic",
    };
    unsigned i;
    for (i = 0; i < sizeof(w) / sizeof(w[0]); i++)
        if (!strcmp(w[i], tx)) return 1;
    return 0;
}

/* 字段基础类型 kind: base = 单个类型名 ("float"/"int"/"short"/"double"/...). */
static int dg_refl_kind_tok(const char *tx)
{
    if (!strcmp(tx, "float")) return DGR_FLOAT;
    if (!strcmp(tx, "double")) return DGR_DOUBLE;
    if (!strncmp(tx, "long long", 9)) return DGR_LLONG;
    if (!strncmp(tx, "short", 5)) return DGR_SHORT;
    if (!strcmp(tx, "int")) return DGR_INT;
    if (!strncmp(tx, "char", 4)) return DGR_BYTE;
    if (!strcmp(tx, "_Bool")) return DGR_BOOL;
    if (!strcmp(tx, "void")) return DGR_VOID;
    return DGR_OTHER;
}

/* 收集本行 struct/union/enum 定义字段 (t051 均为单行 `struct X { ... };`).
 * 解析 `{ }` 间字段声明, 登记 tag; 不拦截落地(struct 定义仍 verbatim 保留). */
static void dg_reflect_collect_line(void)
{
    int i, openb = -1, closeb = -1, kw = 0;
    char tag[128] = { 0 };
    DgReflect *R;
    /* 前几个非空 token 判定 `struct|union|enum <tag> {` */
    for (i = 0; i < dg_n; i++) {
        const char *tx;
        if (is_space(dg_buf[i])) continue;
        tx = dg_txt[i] ? dg_txt[i] : "";
        if (!strcmp(tx, "struct")) kw = 'S';
        else if (!strcmp(tx, "union")) kw = 'U';
        else if (!strcmp(tx, "enum")) kw = 'E';
        else if (kw) { snprintf(tag, sizeof tag, "%s", tx); i++; break; }
        else break;                     /* 非结构体定义行 */
    }
    if (!kw || !tag[0])
        return;
    for (i = 0; i < dg_n; i++) {
        if (is_space(dg_buf[i])) continue;
        if (dg_tokchar(i) == '{' && openb < 0) openb = i;
        else if (dg_tokchar(i) == '}' && openb >= 0) { closeb = i; break; }
    }
    if (openb < 0 || closeb < 0 || openb >= closeb)
        return;
    if (dg_reflect_find(tag, kw) || dg_refln >= DG_REFLECT_MAXT)
        return;
    R = &dg_refl[dg_refln++];
    memset(R, 0, sizeof *R);
    snprintf(R->tag, sizeof R->tag, "%s", tag);
    R->kw = (char)kw;
    if (kw == 'E')                     /* 枚举: 无字段, nf=0 (仅 kind=ENUM 表) */
        return;
    /* 按 `;` 切分字段组(在 `{}` 内) */
    {
        int gs = openb + 1;
        while (gs < closeb) {
            int j, sem = -1, k, fstart;
            for (j = gs; j < closeb; j++)
                if (!is_space(dg_buf[j]) && dg_tokchar(j) == ';') { sem = j; break; }
            if (sem < 0) sem = closeb;
            if (sem <= gs) { gs = sem + 1; continue; }
            /* --- 阶段1: 类型头 tokens, 直到首个声明符名 --- */
            k = gs;
            while (k < sem && is_space(dg_buf[k])) k++;
            fstart = k;                 /* 类型起点(含 *) */
            {
                char base[256] = { 0 };   /* 基础类型文本("struct Vec3"/"int"/"int*") */
                int isptr = 0, gotname = 0, tlen = 0, bi, in_tag = 0;
                char *bb = base;
                while (k < sem && !gotname) {
                    int u = dg_buf[k];
                    const char *tx = dg_txt[k] ? dg_txt[k] : "";
                    if (is_space(u)) { k++; continue; }
                    if (dg_tokchar(k) == '*') { isptr = 1; if (tlen) { *bb++='*'; tlen++; } k++; continue; }
                    /* struct/union/enum 后紧接的 tag 属类型, 不当声明符名 */
                    if (in_tag) in_tag = 0;
                    else if (!strcmp(tx,"struct")||!strcmp(tx,"union")||!strcmp(tx,"enum")) in_tag = 1;
                    else if (u >= TOK_IDENT && !dg_refl_istypeword(tx)) { gotname = 1; break; }
                    /* 追加类型 token 到 base (空格分隔) */
                    if (tlen && bb>base && bb[-1] != '*') *bb++ = ' ';
                    { int n=(int)strlen(tx); memcpy(bb, tx, n); bb+=n; tlen+=n; }
                    k++;
                }
                *bb = 0;
                /* 数组元素/struct 判断: 在类型头 token 里找 struct/union/enum tag */
                {
                    char subtag[128] = { 0 };
                    int sbt = 0;         /* 1=struct/union 值, 2=enum */
                    for (bi = fstart; bi < k; bi++) {
                        const char *tt = dg_txt[bi] ? dg_txt[bi] : "";
                        if (!strcmp(tt, "struct") || !strcmp(tt, "union")) {
                            int q = bi + 1;
                            while (q < k && is_space(dg_buf[q])) q++;
                            if (q < k && dg_txt[q]) snprintf(subtag, sizeof subtag, "%s", dg_txt[q]);
                            if (subtag[0]) sbt = 1;
                            break;
                        }
                        if (!strcmp(tt, "enum")) { sbt = 2; break; }
                    }
                    /* --- 阶段2: 声明符循环 --- */
                    while (k < sem) {
                        char nm[128] = { 0 };
                        DgReflectField *F;
                        int isarr = 0, cnt = 0;
                        while (k < sem && is_space(dg_buf[k])) k++;
                        while (k < sem && dg_tokchar(k) == '*') { isptr = 1; k++; }
                        while (k < sem && is_space(dg_buf[k])) k++;
                        if (k >= sem || dg_buf[k] < TOK_IDENT) break;
                        snprintf(nm, sizeof nm, "%s", dg_txt[k] ? dg_txt[k] : "");
                        k++;
                        /* 数组 [N] */
                        while (k < sem && is_space(dg_buf[k])) k++;
                        if (k < sem && dg_tokchar(k) == '[') {
                            int e = k + 1;
                            while (e < sem && is_space(dg_buf[e])) e++;
                            if (e < sem && dg_txt[e]) cnt = (int)atoi(dg_txt[e]);
                            isarr = 1;
                            while (e < sem && dg_tokchar(e) != ']') e++;
                            k = (e < sem) ? e + 1 : sem;
                        }
                        if (nm[0] && R->nf < DG_REFLECT_MAXF) {
                            F = &R->f[R->nf++];
                            memset(F, 0, sizeof *F);
                            snprintf(F->nm, sizeof F->nm, "%s", nm);
                            if (isarr) {
                                char esz[192];
                                F->kind = DGR_ARRAY; F->isarr = 1; F->cnt = cnt;
                                if (sbt == 1) { snprintf(F->sub, sizeof F->sub, "%s", subtag); }
                                /* base 为元素类型 */
                                snprintf(F->ft, sizeof F->ft, "%s", base);
                                snprintf(esz, sizeof esz, "sizeof(%s)*%du", base, cnt > 0 ? cnt : 1);
                                snprintf(F->sz, sizeof F->sz, "%s", esz);
                            } else if (isptr) {
                                F->kind = DGR_PTR;
                                {
                                    int blen = (int)strlen(base);
                                    if (blen && base[blen - 1] == '*')
                                        snprintf(F->ft, sizeof F->ft, "%s", base);
                                    else
                                        snprintf(F->ft, sizeof F->ft, "%s*", base);
                                }
                                snprintf(F->sz, sizeof F->sz, "sizeof(%s)", F->ft);
                            } else {
                                F->kind = (sbt == 1) ? DGR_STRUCT :
                                          (sbt == 2) ? DGR_ENUM : dg_refl_kind_tok(base);
                                if (sbt == 1) snprintf(F->sub, sizeof F->sub, "%s", subtag);
                                snprintf(F->ft, sizeof F->ft, "%s", base);
                                snprintf(F->sz, sizeof F->sz, "sizeof(%s)", base);
                            }
                        }
                        /* 逗号 → 下一声明符 */
                        while (k < sem && is_space(dg_buf[k])) k++;
                        if (k < sem && dg_tokchar(k) == ',') { k++; continue; }
                        break;
                    }
                }
            }
            gs = sem + 1;
            (void)fstart;
        }
    }
}

/* emit 反射表: 递归先 emit 嵌套 struct 子表, 保证 sub 引用先声明. */
static void dg_reflect_emit_one(TCCState *s1, const char *tag, char kw)
{
    DgReflect *R = dg_reflect_find(tag, kw);
    int i;
    if (!R || R->emitted)
        return;
    R->emitted = 1;
    for (i = 0; i < R->nf; i++)
        if (R->f[i].sub[0])
            dg_reflect_emit_one(s1, R->f[i].sub, 'S');  /* 子表(值/数组元素 struct) */
    {
        const char *kwd = (R->kw == 'U') ? "union" : (R->kw == 'E') ? "enum" : "struct";
        const char *lkw = (R->kw == 'U') ? "union" : (R->kw == 'E') ? "enum" : "struct";
        int hk = (R->kw == 'U') ? DGR_UNION : (R->kw == 'E') ? DGR_ENUM : DGR_STRUCT;
        if (R->nf == 0) {
            fprintf(s1->ppfp,
                    "static const struct __refl %s_refl = { \"%s\", %du, sizeof(%s %s), _Alignof(%s %s), 0u, 0 };\n",
                    R->tag, R->tag, hk, (R->kw=='E')?"enum":kwd, R->tag, (R->kw=='E')?"enum":kwd, R->tag);
        } else {
            fprintf(s1->ppfp, "static const __refl_field %s_f[%d] = {\n", R->tag, R->nf);
            for (i = 0; i < R->nf; i++) {
                DgReflectField *F = &R->f[i];
                char subx[192];
                if (F->sub[0])
                    snprintf(subx, sizeof subx, "&%s_refl", F->sub);
                else
                    strcpy(subx, "0");
                fprintf(s1->ppfp,
                        "  { \"%s\", %du, offsetof(%s %s, %s), %s, _Alignof(%s), %s, %du },\n",
                        F->nm, (unsigned)F->kind, lkw, R->tag, F->nm,
                        F->sz, F->ft, subx, (unsigned)F->cnt);
            }
            fprintf(s1->ppfp, "};\n");
            fprintf(s1->ppfp, "static const struct __refl %s_refl = { \"%s\", %du, sizeof(%s %s), _Alignof(%s %s), %du, %s_f };\n",
                    R->tag, R->tag, (unsigned)hk, kwd, R->tag, kwd, R->tag, (unsigned)R->nf, R->tag);
        }
    }
}

/* 发射全部已登记反射表 (EOF 前置, 置于用户 struct 定义之后、main/函数定义之前).
 * 仅当文件确实用到 __builtin_reflect 才发射; 否则普通 struct (如 t076 的 inode)
 * 无需且不能引用 __refl/__refl_field (会误连 tcc-reflect.h 未含的接口). */
static void dg_reflect_emit(TCCState *s1)
{
    int i;
    if (!dg_used_reflect)
        return;
    for (i = 0; i < dg_refln; i++)
        dg_refl[i].emitted = 0;
    for (i = 0; i < dg_refln; i++)
        dg_reflect_emit_one(s1, dg_refl[i].tag, dg_refl[i].kw);
}

/* 一次前向扫描产出语句形状 (DgStmt), 取代 dg_flush 里散落的多个状态机 pass.
 * 扫描同步维护 defer 块深度: `{` 依前序有效 token 判定语句块(dg_dep++,
 * 计入 defer 作用域)还是初始化器/聚合(dg_ini++, 不计入); 闭合块把层号依
 * token 序记入 cls[] 供 dispatch 逆序发射 defer. 各"首个命中"独立记录,
 * 精确复刻旧 pass 的 break/落空语义 (见各 *_ok 判定). */
static DgStmt dg_classify(void)
{
    DgStmt st;
    int i, prev = 0, depth = 0;
    memset(&st, 0, sizeof st);
    st.kind = DG_STMT_OTHER;
    st.first = st.bopn = st.bclo = -1;
    st.ca = st.calh = st.casem = -1;
    st.inc = st.incspec = st.incsem = -1;
    st.eq = st.ret = -1;
    for (i = 0; i < dg_n; i++) {
        int ch, t = dg_buf[i];
        if (is_space(t))
            continue;
        if (st.first < 0)
            st.first = i;
        ch = dg_tokchar(i);
        /* --- defer 块深度维护 (语句块计入 dg_dep, 初始化器/聚合由 dg_ini) --- */
        if (ch == '{') {
            if (prev == '=' || prev == ',')
                dg_ini++;
            else if (dg_dep < DG_DEFER_MAXDEP - 1)
                dg_dep++;
        } else if (ch == '}') {
            if (dg_ini > 0)
                dg_ini--;
            else if (dg_dep > 0) {
                if (st.ncls < DG_DEFER_MAXDEP)
                    st.cls[st.ncls++] = dg_dep;
                dg_dep--;
            }
        } else {
            prev = ch;
        }
        /* --- 首 token: if/while 条件区 / 退出语句(return/goto/break) 早退 --- */
        if (st.first == i) {
            if (t == TOK_IF || t == TOK_WHILE) {
                st.ifw = 1;
                st.kind = DG_STMT_IFWHILE;
                st.bopn = dg_next_ns(i);
                st.bclo = (st.bopn >= 0 && dg_tokchar(st.bopn) == '(')
                          ? dg_pair[st.bopn] : -1;
            } else if (t == TOK_RETURN) {
                st.isret = 1;
            } else if (t == TOK_GOTO || t == TOK_BREAK || t == TOK_CONTINUE) {
                st.isexit = 1;   /* goto / break / continue 跳出作用域: 先发 defer */
            }
        }
        /* --- 复合赋值: 首个 += -= *= /= %= --- */
        if (st.ca < 0 && (t == TOK_A_ADD || t == TOK_A_SUB || t == TOK_A_MUL
                          || t == TOK_A_DIV || t == TOK_A_MOD)) {
            int base = 0, id, lh = -1, sem = -1, j;
            switch (t) {
            case TOK_A_ADD: base = '+'; break;
            case TOK_A_SUB: base = '-'; break;
            case TOK_A_MUL: base = '*'; break;
            case TOK_A_DIV: base = '/'; break;
            case TOK_A_MOD: base = '%'; break;
            }
            st.ca = i;              /* 首个 CA token 锁定 (旧 pass 在此 break) */
            id = dg_opid(base);
            if (id && dg_oreg[id]) {
                for (j = i - 1; j >= 0; j--)
                    if (!is_space(dg_buf[j])) { lh = j; break; }
                for (j = i + 1; j < dg_n; j++)
                    if (dg_tokchar(j) == ';') { sem = j; break; }
                if (lh >= 0 && sem >= 0 && dg_var_of(dg_buf[lh])) {
                    st.cabase = base; st.calh = lh; st.casem = sem;
                    st.ca_ok = 1;
                    if (st.kind == DG_STMT_OTHER)
                        st.kind = DG_STMT_CA;
                }
            }
        }
        /* --- 自增减: 首个 ++ / -- --- */
        if (st.inc < 0 && (t == TOK_INC || t == TOK_DEC)) {
            int id = dg_opid(t), pn = -1, nn, sem = -1, spec, j;
            st.inc = i;             /* 首个 INC/DEC token 锁定 (旧 pass 在此 break) */
            if (id && dg_oreg[id]) {
                for (j = i - 1; j >= 0; j--)
                    if (!is_space(dg_buf[j])) { pn = j; break; }
                nn = dg_next_ns(i);
                for (j = dg_n - 1; j >= 0; j--)
                    if (!is_space(dg_buf[j])) { sem = j; break; }
                if (sem >= 0 && dg_tokchar(sem) == ';' && nn >= 0) {
                    if (pn >= 0 && nn == sem)
                        spec = pn;          /* 后缀 a++ */
                    else if (pn == -1)
                        spec = nn;          /* 前缀 ++a */
                    else
                        spec = -1;          /* 嵌入表达式 -> verbatim */
                    if (spec >= 0 && dg_var_of(dg_buf[spec])) {
                        st.incspec = spec; st.incsem = sem;
                        st.inc_ok = 1;
                        if (st.kind == DG_STMT_OTHER)
                            st.kind = DG_STMT_INCDEC;
                    }
                }
            }
        }
        /* --- 顶层 '=' 与 return (仅 [] 计入深度, 与旧行为一致) --- */
        if (ch == '[')
            depth++;
        else if (ch == ']') {
            if (depth > 0)
                depth--;
        } else if (t == '=' && depth == 0 && st.eq < 0) {
            st.eq = i;
            if (st.kind == DG_STMT_OTHER)
                st.kind = DG_STMT_ASSIGN;
        } else if (t == TOK_RETURN && st.ret < 0) {
            st.ret = i;
            if (st.kind == DG_STMT_OTHER)
                st.kind = DG_STMT_RETURN;
        }
    }
    return st;
}

static void dg_flush(TCCState *s1)
{
    int i, start = -1, to;
    DgStmt st;
    /* 一次性构建本行配对索引, 供下述各改写 pass 共享 */
    dg_build_pairs();
    /* reflect: 收集本行 struct/enum 定义字段 (生成反射表用, 不拦截落地) */
    dg_reflect_collect_line();
    /* ===== 泛型对象方法糖改写: recv->mname(targs)(args) → mname(targs)(&recv,args) ===== */
    dg_sugar_rewrite(s1);
    /* sugar 改写会重建 token 数组, 配对索引随之失效 —— 重建 */
    dg_build_pairs();
    /* P1: 收集 operator 定义行的操作数基础类型名(供泛型体改写判断 T 是否 op 类型) */
    dg_optyp_collect_line();
    /* ===== model 定义收集 (语句级, 先于 operator/defer) =====
     *  `model struct Eq(T) { ... };` 不落地: 收进 dgm 后登记到 dg_model_def,
     *  实例化点 `Eq(int) x` 由 dg_emit_verbatim 改写为合成 typedef. */
    if (dg_mc || dg_is_model_line()) {
        if (!dg_mc) {       /* 本行开始一段 model 定义 */
            dgm_n = 0;      /* dgm 数组可直接复用, 只重置长度 */
            dg_mbr = 0;
            dg_msemi = 0;
            dg_mbody = 0;
            dg_mbase = -1;
            dg_mc = 1;
        }
        if (!dg_model_collect(s1))
            return;         /* 跨行仍未闭合: 不落地(dgm 保留续接), 待下行续接 */
        dg_model_finish();   /* 已闭合: 登记模板, 定义本身不落地 */
        dg_mc = 0;
        dgm_free();
        return;
    }
    /* ===== defer 处理 (语句级, 先于 operator) =====
     *  行内任意位置出现 `defer f(a);`: 不落地, 按块深度入栈, 由闭块 `}` 逆序重放
     *  (与 TCC "离开作用域逆序执行" 语义一致). 支持同行多 defer / `{`/`}` 混排. */
    if (dg_has_defer()) {
        dg_flush_defer_line(s1);
        return;                     /* defer 自身不落地, 交由闭块重放 */
    }
    /* 元数据 pass: 注册 operator 定义 / 收集 struct 变量声明类型.
     * 登记所有 `struct <tag> <decl>[, <decl>...]` 的声明符(token >= TOK_IDENT 即
     * 用户标识符)为 operator 类型变量, 覆盖局部变量与函数参数; 遇到另一类型
     * 关键字/`{` 初始化器/`(` 调用/对应 operator 关键字则结束当前声明列表. */
    {
        int prev = 0, pp = 0, pending = 0, star = 0;
        for (i = 0; i < dg_n; i++) {
            int t, ch;
            if (is_space(dg_buf[i]))
                continue;             /* 保持 prev/pp/pending 为纯净非空格 token */
            t = dg_buf[i]; ch = dg_tokchar(i);
            if (ch == '*')
                star = 1;             /* A* 指针指示(延迟到变量登记时消费) */
            /* 先置 pending: `struct <tag>`(刚刚见 prev 是 tag) 已确立 */
            if (pp == TOK_STRUCT && prev >= TOK_IDENT) {
                pending = prev;
                if (ch != '*')        /* '*' 自身确立 pending 时保留指针指示 */
                    star = 0;
            }
            if (t == TOK_OPERATOR) {
                int opid = dg_opat(i);
                if (opid) {
                    dg_active = 1;
                    if (prev >= TOK_IDENT)      /* prev 为返回类型 tag */
                        dg_op_tag[opid] = prev;
                }
                pending = 0;
            } else if (t == TOK_STRUCT) {
                pending = 0;            /* 紧随的 tag 下面经 pp==TOK_STRUCT 置入 */
            } else if (ch == '{' || ch == '(') {
                pending = 0;            /* 初始化器 / 调用 / 形参列表终止声明 */
            } else if (t >= TOK_IDENT) {        /* 用户标识符 */
                int wid = dg_opat_word(i);      /* operator_eq 等标识符形式运算符 */
                if (wid) {
                    dg_active = 1;
                    if (prev >= TOK_IDENT)      /* prev 为返回类型 tag */
                        dg_op_tag[wid] = prev;
                } else {
                    const char *w = dg_txt[i] ? dg_txt[i] : "";
                    if (!pending && dg_optyp_has(w)) {
                        pending = t;    /* typedef operator 类型名(如 STL_string):
                                          后续声明符登记为 op 类型, 触发运算符改写 */
                    } else if (dg_w_intkey(w) || !strcmp(w, "float") ||
                        !strcmp(w, "double") || !strcmp(w, "void") ||
                        !strcmp(w, "_Bool"))
                        pending = 0;   /* 标量类型关键字终止 struct 声明列表 */
                    else if (pending) {
                        /* 登记变量; 指针性按"最近声明"覆盖 —— 同名(如函数形参
                         * `struct Point *a` 与局部 `struct Point a`)以作用域内
                         * 最新一次声明为准(C 先声明后用, 文本序近似作用域).
                         * 顺序: 先 dg_add_var 落槽(槽内 ptr 初始 0), 再
                         * dg_var_setptr2 写入 star —— 若先 setptr2 后 add,
                         * add 的 0 会覆盖首次声明的指针标记, 丢失 A*. */
                        if (!dg_var_of(t))
                            dg_add_var(t, pending);
                        dg_var_setptr2(t, star);
                        star = 0;
                    }
                }
            } else if (ch == ';') {
                pending = 0;            /* 声明结束 */
            } else if (ch != ',' && ch != '*' && ch != '[' && ch != ']') {
                pending = 0;            /* 其它关键字/标点(如 int/float/=)终止声明列表 */
            }
            pp = prev; prev = t;
        }
    }

    /* ===== 分类: 一次前向扫描产出语句形状 (DgStmt), 同步维护 defer 块深度
     * (语句块计入 dg_dep, 初始化器/聚合由 dg_ini 跟踪, 跨行存活); 本行闭合的
     * 语句块层依 token 序记入 st.cls, 早退 return 记入 st.isret. ===== */
    st = dg_classify();
    for (i = 0; i < st.ncls; i++)
        dg_emit_defer_level(s1, st.cls[i]);
    /* 早退 return: 本行以 `return` 开头时, 先把已进入作用域的 defer 逐层逆序
     * 落地 (内层先), 再让下方 emit 路径把 return 行原样落地 —— 使 return 离开
     * 函数时也能执行清理. goto/break/continue 跳出作用域同理 (t029 的 goto 跨块
     * 与 for 内 break 均依赖此在跳出前发射, 否则块末 `}` 的 defer 成死代码). */
    if (st.isret || st.isexit)
        dg_emit_defer_all(s1, dg_dep);

    /* ===== 单一分发 ===== */
    /* ===== if/while 条件改写: `if (cond)` / `while (cond)` 的括号条件区,
     * 命中 operator 比较/一元的展开 (operator_eq(a,b) 等). ===== */
    if (st.ifw) {
        if (st.bclo >= 0) {
            dg_emit_verbatim(s1, 0, st.bopn + 1);      /* `if (` */
            if (!dg_region_rewrite(s1, st.bopn + 1, st.bclo))
                dg_emit_verbatim(s1, st.bopn + 1, st.bclo);
            dg_emit_verbatim(s1, st.bclo, dg_n);       /* `) { ...` 续写 */
        } else {
            dg_emit_verbatim(s1, 0, dg_n);
        }
        return;
    }

    /* ===== 复合赋值改写: `a op= b` (op ∈ + - * / %) → `a = operator_<wrd>(a, b);`
     * 仅当 base 二元算子已注册且 LHS 为 operator 类型变量; 否则 verbatim. ===== */
    if (st.ca_ok) {
        int id = dg_opid(st.cabase);
        const char *__t = get_tok_str(dg_var_of(dg_buf[st.calh]), NULL);
        dg_emit_verbatim(s1, 0, st.ca);             /* LHS `a ` */
        fputs("= ", s1->ppfp);
        fputs(dg_op_nm_txt(id, __t), s1->ppfp);
        fputs("(", s1->ppfp);
        fputs(dg_txt[st.calh], s1->ppfp);
        fputs(", ", s1->ppfp);
        if (!dg_region_rewrite(s1, st.ca + 1, st.casem))
            dg_emit_verbatim(s1, st.ca + 1, st.casem);
        fputs(");", s1->ppfp);
        dg_emit_verbatim(s1, st.casem + 1, dg_n);   /* 尾部(通常空) */
        return;
    }

    /* ===== 自增自减改写: `++a` / `a++` / `--a` / `a--` (operator 类型) →
     * `a = operator_inc(a);` / `a = operator_dec(a);`. 仅处理整行仅为
     * 自增减表达式[;] 的语句 (前后缀统一存回新值, 与 TCC -run 值语义一致). ===== */
    if (st.inc_ok) {
        int id = dg_opid(dg_buf[st.inc]);
        const char *__t = get_tok_str(dg_var_of(dg_buf[st.incspec]), NULL);
        fputs(dg_txt[st.incspec], s1->ppfp);
        fputs(" = ", s1->ppfp);
        fputs(dg_op_nm_txt(id, __t), s1->ppfp);
        fputs("(", s1->ppfp);
        fputs(dg_txt[st.incspec], s1->ppfp);
        fputs(");", s1->ppfp);
        dg_emit_verbatim(s1, st.incsem + 1, dg_n);
        return;
    }

    /* ===== return/赋值表达式改写: 顶层赋值 '=' 与**行首** `return` (token 恰为单
     * '=' 字符, 排除 '==' 等; 仅 [] 计入深度, 与旧行为一致).
     * 注意: 仅当整行以 return 开头(st.isret)才走 return 分支 —— 宏展开的
     * `do { if (!(c)) return N; } while(0);` 等行虽含内嵌 return 但 st.isret=0,
     * 必须落入下方整行语句级改写 (扁平 token 扫描), 才能改到内嵌比较运算. ===== */
    if (st.isret || st.eq >= 0) {
        /* 确定候选表达式区域: 优先 `return <expr>;`, 否则顶层赋值右值 */
        if (st.isret)
            start = dg_next_ns(st.ret);
        else
            start = st.eq + 1;
        if (start < 0 || start >= dg_n) {
            /* 无有效右值: 若是 operator 类型变量的二元运算行
             * (如 `CHECK(x == same);`), 交 dg_line_rewrite 做语句级运算符改写. */
            if (dg_line_rewrite(s1))
                return;
            dg_emit_verbatim(s1, 0, dg_n);
            return;
        }
        /* region 上界: 首个顶层 ';' 之后 */
        to = dg_n;
        for (i = start; i < dg_n; i++)
            if (dg_tokchar(i) == ';') {
                to = i;
                break;
            }
        if (to < start)
            to = dg_n;

        if (st.ret >= 0) {
            /* return <expr>: 前缀(含 return 与空白) verbatim, 区域内改写 */
            dg_emit_verbatim(s1, 0, start);
            if (!dg_region_rewrite(s1, start, to))
                dg_emit_verbatim(s1, start, dg_n);
            else if (to < dg_n)
                dg_emit_verbatim(s1, to, dg_n);
            return;
        }
        /* 顶层赋值: LHS verbatim, 手动补 '= ', RHS 区域内改写 */
        dg_emit_verbatim(s1, 0, st.eq);
        fputs("= ", s1->ppfp);
        if (!dg_region_rewrite(s1, start, to))
            dg_emit_verbatim(s1, start, dg_n);
        else if (to < dg_n)
            dg_emit_verbatim(s1, to, dg_n);
        return;
    }
    /* ===== 语句级二元算子改写 (通用兜底, 于复合赋值/自增减/赋值=return 特例之后):
     * 未命中上述路径的整行 (如 `CHECK(x == same);` 宏展开 / 纯比较语句 / 函数实参
     * 内运算), 由扁平 token 扫描把两侧均为 operator 类型变量的二元算子改写成
     * operator_<wrd>_<type>(l,r). 注意: 赋值/return 行已由 dg_region_rewrite 走
     * AST 优先级树优先处理, 此处不再抢先, 避免嵌套/混合优先级被拍平. ===== */
    if (dg_line_rewrite(s1))
        return;

    /* 兜底 verbatim */
    dg_emit_verbatim(s1, 0, dg_n);
}

static void dg_step(TCCState *s1, int tok, int *token_seen, char *white, int *spcs)
{
    int i;
    /* 统一缓冲整行到 LINEFEED 再整体 flush:
     *  - 无 operator 的编译: flush 走 verbatim 精确重放(空格 token 原样),
     *    输出与 --emit-c 现有行为一致 -> 零回归 (t052/simd_demo/t046).
     *  - 有 operator 定义: flush 先跑元数据收集(定义改名/变量类型),
     *    再对简单赋值右值做完全括号忠实改写. */
    if (tok == TOK_LINEFEED) {
        if (dg_n > 0) {
            dg_flush(s1);
            for (i = 0; i < dg_n; i++)      /* 释放文本快照 */
                if (dg_txt[i]) {
                    tcc_free(dg_txt[i]);
                    dg_txt[i] = NULL;
                }
            dg_n = 0;
        }
        fputc('\n', s1->ppfp);
        ++file->line_ref;
        *token_seen = TOK_LINEFEED;
        return;
    }
    if (dg_n < DG_BUFN) {
        const char *s = get_tok_str(tok, &tokc);
        size_t n;
        if (!s)
            s = "";
        n = strlen(s);
        dg_buf[dg_n] = tok;
        dg_txt[dg_n] = tcc_malloc(n + 1);
        memcpy(dg_txt[dg_n], s, n + 1);
        dg_n++;
    }
}


/* 预处理输出共用引擎: -E 与 --emit-c 都走这里.
 * desugar != 0 时按脱糖语义处理(产物顶部注入来源标记, 预留扩展改写挂点). */
static int preprocess_loop(TCCState *s1, int desugar)
{
    BufferedFile **iptr;
    int token_seen, spcs, level;
    const char *p;
    char white[400];

    parse_flags = PARSE_FLAG_PREPROCESS
                | (parse_flags & PARSE_FLAG_ASM_FILE)
                | PARSE_FLAG_LINEFEED
                | PARSE_FLAG_SPACES
                | PARSE_FLAG_ACCEPT_STRAYS
                ;
    if (s1->do_bench) {
        do next(); while (tok != TOK_EOF);
        return 0;
    }

    token_seen = TOK_LINEFEED, spcs = 0, level = 0;
    if (file->prev)
        pp_line(s1, file->prev, level++);
    pp_line(s1, file, level);

    if (desugar) {
        /* 脱糖产物: 注入来源标记; SIMD 头由用户源码自行 #include,
         * 保证产物 = 标准 C, clang 可原生编译而不注入额外依赖.
         * 函数泛型: 合成定义须物理置于其调用点(通常在函数体内)之前, 才能作为
         * 原型被 gcc 识别; 故整段主体先缓冲到临时文件, EOF 处回放
         * [typedefs+函数泛型定义][主体], 保证调用点先见原型. */
        dg_reset();
        dg_saved_ppfp = s1->ppfp;
        fprintf(dg_saved_ppfp, "\n/* __TCC_DESUGAR__: 由 tcc --emit-c 生成的标准C产物 */\n");
        dg_tmpfile = tmpfile();
        dg_buffering = 1;
        s1->ppfp = dg_tmpfile;
    }

    for (;;) {
        iptr = s1->include_stack_ptr;
        next();
        if (tok == TOK_EOF)
            break;
        level = s1->include_stack_ptr - iptr;
        if (level) {
            if (level > 0)
                pp_line(s1, *iptr, 0);
            pp_line(s1, file, level);
        }
        if (s1->dflag & 7) {
            pp_debug_defines(s1);
            if (s1->dflag & 4)
                continue;
        }

        /* 脱糖扩展改写挂点: SIMD/intrinsic 直透传; operator 在 dg_step 内
         * 做 token 级改写(定义改名 + 赋值右值忠实改写). */
        if (desugar) {
            dg_step(s1, tok, &token_seen, white, &spcs);
            continue;
        }

        if (is_space(tok)) {
            if (spcs < sizeof white - 1)
                white[spcs++] = tok;
            continue;
        } else if (tok == TOK_LINEFEED) {
            spcs = 0;
            if (token_seen == TOK_LINEFEED)
                continue;
            ++file->line_ref;
        } else if (token_seen == TOK_LINEFEED) {
            pp_line(s1, file, 0);
        } else if (spcs == 0 && pp_need_space(token_seen, tok)) {
            white[spcs++] = ' ';
        }

        white[spcs] = 0, fputs(white, s1->ppfp), spcs = 0;
        fputs(p = get_tok_str(tok, &tokc), s1->ppfp);
        token_seen = pp_check_he0xE(tok, p);
    }
    if (desugar && dg_tmpfile) {
        /* --emit-c 收尾: 整体读入缓冲主体, 把函数泛型 typedefs+定义插到
         * "最后一个回到主文件的行标记"之后(此时引号头如 allocator.h 已展开,
         * STL_Arena/size_t 等依赖类型均可见; 若直接放在文件最顶会因前向引用
         * 未定义类型而失败). 保证 main 及所有调用点先见到这些定义(充当原型). */
        static char tmp[65536];
        CString body;
        size_t n, i;
        int ins;
        cstr_new(&body);
        fflush(dg_tmpfile);
        rewind(dg_tmpfile);
        while ((n = fread(tmp, 1, sizeof tmp, dg_tmpfile)) > 0)
            cstr_cat(&body, tmp, n);
        fclose(dg_tmpfile);
        dg_tmpfile = NULL;
        dg_buffering = 0;
        s1->ppfp = dg_saved_ppfp;
        /* 优先把函数泛型 typedefs+定义插到 `int main` 之前: 保证其引用的用户
         * 顶层实体(struct Cmp / operator_lt 等, 主文件直接代码)已先定义. 若用
         * "最后一个回到主文件行标记"作插点, 会插在用户主文件代码(main 前的
         * struct 定义/顶层函数)之前, 使函数泛型引用未定义类型 → incomplete type. */
        ins = -1;
        {
            const char *pats[2] = { "int  main", "int main" };
            int pi;
            for (pi = 0; pi < 2 && ins < 0; pi++) {
                size_t plen = strlen(pats[pi]);
                if (body.size >= plen) {
                    size_t k = body.size - plen;
                    for (;;) {
                        if (!memcmp(body.data + k, pats[pi], plen)) { ins = (int)k; break; }
                        if (k == 0) break;
                        k--;
                    }
                }
            }
        }
        /* 结构 typedef 前置插点: 最后一个回到主文件的行标记之后(header 展开末尾),
         * 依赖的库类型(stl_iter_ops/STL_Arena)已展开可见, 且早于所有用户顶层代码
         * (t076 顶层 i_incr 用合成类型即由此先见定义). 函数泛型定义仍插 int main 前. */
        {
            int hdr = 0;
            for (i = body.size; i >= 4; i--)
                if (body.data[i - 4] == '"' && body.data[i - 3] == ' '
                    && body.data[i - 2] == '2' && body.data[i - 1] == '\n') {
                    hdr = (int)i; break;
                }
            if (ins < 0)
                ins = hdr;              /* 无 main: 函数定义紧随 typedef 段 */
            if (hdr > ins)
                hdr = ins;
            fwrite(body.data, 1, hdr, dg_saved_ppfp);              /* A: header+回主文件标记 */
            dg_fdefs_typedefs(s1);                                 /* 结构/nested typedef */
            fwrite(body.data + hdr, 1, ins - hdr, dg_saved_ppfp);  /* B: 用户顶层(main 前) */
            dg_reflect_emit(s1);                                   /* 反射表(用户 struct 定义后) */
            dg_fdefs_funcs(s1);                                    /* 函数泛型定义 */
            fwrite(body.data + ins, 1, body.size - ins, dg_saved_ppfp); /* C: main 及后 */
        }
        cstr_free(&body);
    }
    return 0;
}

ST_FUNC int tcc_preprocess(TCCState *s1)
{
    /* tcc -E: 前置 Pflag(P10 数字 token) 处理, 然后进共享循环 */
    if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_P10)
        parse_flags |= PARSE_FLAG_TOK_NUM, s1->Pflag = 1;
    return preprocess_loop(s1, 0);
}

ST_FUNC int tcc_desugar(TCCState *s1)
{
    return preprocess_loop(s1, 1);
}

/* ------------------------------------------------------------------------- */
