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

/********************************************************/
/* global variables */

/* loc : local variable index
   ind : output code index
   rsym: return symbol
   anon_sym: anonymous symbol index
*/
ST_DATA int rsym, anon_sym, ind, loc;

ST_DATA Sym *global_stack;
ST_DATA Sym *local_stack;
ST_DATA Sym *define_stack;
ST_DATA Sym *global_label_stack;
ST_DATA Sym *local_label_stack;

static Sym *sym_free_first;
static void **sym_pools;
static int nb_sym_pools;

static Sym *all_cleanups, *pending_gotos;
static int local_scope;
ST_DATA char debug_modes;

ST_DATA SValue *vtop;
static SValue _vstack[1 + VSTACK_SIZE];
#define vstack (_vstack + 1)

ST_DATA int nocode_wanted; /* no code generation wanted */
#define NODATA_WANTED (nocode_wanted > 0) /* no static data output wanted either */
#define DATA_ONLY_WANTED 0x80000000 /* ON outside of functions and for static initializers */

/* no code output after unconditional jumps such as with if (0) ... */
#define CODE_OFF_BIT 0x20000000
#define CODE_OFF() if(!nocode_wanted)(nocode_wanted |= CODE_OFF_BIT)
#define CODE_ON() (nocode_wanted &= ~CODE_OFF_BIT)

/* no code output when parsing sizeof()/typeof() etc. (using nocode_wanted++/--) */
#define NOEVAL_MASK 0x0000FFFF
#define NOEVAL_WANTED (nocode_wanted & NOEVAL_MASK)

/* no code output when parsing constant expressions */
#define CONST_WANTED_BIT  0x00010000
#define CONST_WANTED_MASK 0x0FFF0000
#define CONST_WANTED  (nocode_wanted & CONST_WANTED_MASK)

ST_DATA int global_expr;  /* true if compound literals must be allocated globally (used during initializers parsing */
ST_DATA CType func_vt; /* current function return type (used by return instruction) */
ST_DATA int func_var; /* true if current function is variadic (used by return instruction) */
ST_DATA int func_vc; /* stack address for implicit struct return storage */
ST_DATA int func_ind; /* function start address */
static int func_old;
ST_DATA const char *funcname;
ST_DATA CType int_type, func_old_type, char_type, char_pointer_type;
static CString initstr;

#if PTR_SIZE == 4
#define VT_SIZE_T (VT_INT | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_INT
#elif LONG_SIZE == 4
#define VT_SIZE_T (VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_LLONG
#else
#define VT_SIZE_T (VT_LONG | VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T (VT_LONG | VT_LLONG)
#endif

static struct switch_t {
    struct case_t {
        int64_t v1, v2;
        int ind, line;
    } **p; int n; /* list of case ranges */
    int def_sym; /* default symbol */
    int nocode_wanted;
    int *bsym;
    struct scope *scope;
    struct switch_t *prev;
    SValue sv;
} *cur_switch; /* current switch */

#define MAX_TEMP_LOCAL_VARIABLE_NUMBER 8
/*list of temporary local variables on the stack in current function. */
static struct temp_local_variable {
	int location; //offset on stack. Svalue.c.i
	short size;
	short align;
} arr_temp_local_vars[MAX_TEMP_LOCAL_VARIABLE_NUMBER];
static int nb_temp_local_vars;

static struct scope {
    struct scope *prev;
    struct { int loc, locorig, num; } vla;
    struct { Sym *s; int n; } cl;
    int *bsym, *csym;
    Sym *lstk, *llstk;
} *cur_scope, *loop_scope, *root_scope;

/* defer 语句记录: 参数类型/偏移/记录区帧偏移 (Go 式: 注册点求值) */
#define MAX_DEFER_ARGS 8
#define MAX_DEFER_DESC 64
typedef struct DeferDesc {
    Sym *fn;                            /* 被调函数符号 */
    int nargs;                          /* 参数个数 */
    CType argtypes[MAX_DEFER_ARGS];     /* 参数类型 (转换后) */
    int argoffs[MAX_DEFER_ARGS];        /* 参数在记录区 D 中的偏移 */
    int d_off;                          /* 记录区 D 的帧偏移 */
} DeferDesc;
static DeferDesc defer_descs[MAX_DEFER_DESC];
static int nb_defer_descs;

typedef struct {
    Section *sec;
    int local_offset;
    Sym *flex_array_ref;
} init_params;

#if 1
#define precedence_parser
static void init_prec(void);
#endif

static void block(int flags);
#define STMT_EXPR 1
#define STMT_COMPOUND 2

static void gen_cast(CType *type);
static void gen_cast_s(int t);
static inline CType *pointed_type(CType *type);
static int is_compatible_types(CType *type1, CType *type2);
static void struct_decl(CType *type, int u);
static int parse_btype(CType *type, AttributeDef *ad, int ignore_label);
static CType *type_decl(CType *type, AttributeDef *ad, int *v, int td);
static void parse_expr_type(CType *type);
static void init_putv(init_params *p, CType *type, unsigned long c);
static void decl_initializer(init_params *p, CType *type, unsigned long c, int flags);
static void decl_initializer_alloc(CType *type, AttributeDef *ad, int r, int has_init, int v, int scope);
static int decl(int l);
static void expr_eq(void);
static void vpush_type_size(CType *type, int *a);
static int is_compatible_unqualified_types(CType *type1, CType *type2);
static inline int64_t expr_const64(void);
static void vpush64(int ty, unsigned long long v);
static void vpush(CType *type);
static int gvtst(int inv, int t);
static void gen_inline_functions(TCCState *s);
static void free_inline_functions(TCCState *s);
static void skip_or_save_block(TokenString **str);
static void gv_dup(void);
static int get_temp_local_var(int size,int align,int *r2);
static void defer_statement(void);
static void emit_defer_call(struct DeferDesc *dd);
static void gen_function(Sym *sym);
static void method_compile_all(void);
static int method_lookup(int ident);
static void model_fn_compile_all(void);
static void cast_error(CType *st, CType *dt);
static void end_switch(void);
static void do_Static_assert(void);

/* ------------------------------------------------------------------------- */
/* Automagical code suppression */

/* Clear 'nocode_wanted' at forward label if it was used */
ST_FUNC void gsym(int t)
{
  if (t) {
    gsym_addr(t, ind);
    CODE_ON();
  }
}

/* Clear 'nocode_wanted' if current pc is a label */
static int gind()
{
  int t = ind;
  CODE_ON();
  if (debug_modes)
    tcc_tcov_block_begin(tcc_state);
  return t;
}

/* Set 'nocode_wanted' after unconditional (backwards) jump */
static void gjmp_addr_acs(int t)
{
  gjmp_addr(t);
  CODE_OFF();
}

/* Set 'nocode_wanted' after unconditional (forwards) jump */
static int gjmp_acs(int t)
{
  t = gjmp(t);
  CODE_OFF();
  return t;
}

/* These are #undef'd at the end of this file */
#define gjmp_addr gjmp_addr_acs
#define gjmp gjmp_acs
/* ------------------------------------------------------------------------- */

ST_INLN int is_float(int t)
{
    int bt = t & VT_BTYPE;
    return bt == VT_LDOUBLE
        || bt == VT_DOUBLE
        || bt == VT_FLOAT
        || bt == VT_QFLOAT;
}

static inline int is_integer_btype(int bt)
{
    return bt == VT_BYTE
        || bt == VT_BOOL
        || bt == VT_SHORT
        || bt == VT_INT
        || bt == VT_LLONG;
}

static int btype_size(int bt)
{
    return bt == VT_BYTE || bt == VT_BOOL ? 1 :
        bt == VT_SHORT ? 2 :
        bt == VT_INT ? 4 :
        bt == VT_LLONG ? 8 :
        bt == VT_PTR ? PTR_SIZE : 0;
}

/* returns function return register from type */
static int R_RET(int t)
{
    if (!is_float(t))
        return REG_IRET;
#ifdef TCC_TARGET_X86_64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return TREG_ST0;
#elif defined TCC_TARGET_RISCV64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return REG_IRET;
#endif
    return REG_FRET;
}

/* returns 2nd function return register, if any */
static int R2_RET(int t)
{
    t &= VT_BTYPE;
#if PTR_SIZE == 4
    if (t == VT_LLONG)
        return REG_IRE2;
#elif defined TCC_TARGET_X86_64
    if (t == VT_QLONG)
        return REG_IRE2;
    if (t == VT_QFLOAT)
        return REG_FRE2;
#elif defined TCC_TARGET_RISCV64
    if (t == VT_LDOUBLE)
        return REG_IRE2;
#endif
    return VT_CONST;
}

/* returns true for two-word types */
#define USING_TWO_WORDS(t) (R2_RET(t) != VT_CONST)

/* put function return registers to stack value */
static void PUT_R_RET(SValue *sv, int t)
{
    sv->r = R_RET(t), sv->r2 = R2_RET(t);
}

/* returns function return register class for type t */
static int RC_RET(int t)
{
    return reg_classes[R_RET(t)] & ~(RC_FLOAT | RC_INT);
}

/* returns generic register class for type t */
static int RC_TYPE(int t)
{
    if (!is_float(t))
        return RC_INT;
#ifdef TCC_TARGET_X86_64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return RC_ST0;
    if ((t & VT_BTYPE) == VT_QFLOAT)
        return RC_FRET;
#elif defined TCC_TARGET_RISCV64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return RC_INT;
#endif
    return RC_FLOAT;
}

/* returns 2nd register class corresponding to t and rc */
static int RC2_TYPE(int t, int rc)
{
    if (!USING_TWO_WORDS(t))
        return 0;
#ifdef RC_IRE2
    if (rc == RC_IRET)
        return RC_IRE2;
#endif
#ifdef RC_FRE2
    if (rc == RC_FRET)
        return RC_FRE2;
#endif
    if (rc & RC_FLOAT)
        return RC_FLOAT;
    return RC_INT;
}

/* we use our own 'finite' function to avoid potential problems with
   non standard math libs */
/* XXX: endianness dependent */
ST_FUNC int ieee_finite(double d)
{
    int p[4];
    memcpy(p, &d, sizeof(double));
    return ((unsigned)((p[1] | 0x800fffff) + 1)) >> 31;
}

ST_FUNC void test_lvalue(void)
{
    if (!(vtop->r & VT_LVAL))
        expect("lvalue");
}

ST_FUNC void check_vstack(void)
{
    if (vtop != vstack - 1)
        tcc_error("internal compiler error: vstack leak (%d)",
                  (int)(vtop - vstack + 1));
}

#if 0
/* dump 'b' vstack entries starting with 'a'  */
void pv (const char *lbl, int a, int b)
{
    int i;
    for (i = a; i < a + b; ++i) {
        SValue *p = &vtop[-i];
        printf("%s vtop[-%d] : type.t:%04x  r:%04x  r2:%04x  c.i:%d\n",
            lbl, i, p->type.t, p->r, p->r2, (int)p->c.i);
    }
}

/* dump symbols on stack from s ... last */
static inline void psyms(const char *msg, Sym *s, Sym *last)
{
    printf("%-8s scope         v        c        r   type.t\n", msg);
    while (s && s != last) {
        printf("      %8x  %08x %08x %08x %08x %s\n",
            s->sym_scope, s->v, s->c, s->r, s->type.t, get_tok_str(s->v, 0));
        s = s->prev;
    }
}

static void type_to_str(char *buf, int buf_size, CType *type, const char *varstr);

/* print type */
static void ptype(const char *msg, CType *type, int v)
{
    char buf[500];
    type_to_str(buf, sizeof(buf), type,
        (v & ~SYM_FIELD) ? get_tok_str(v, NULL) : NULL);
    printf("%s : %s;\n", msg, buf);
}
#endif

/* ------------------------------------------------------------------------- */
/* initialize vstack and types.  This must be done also for tcc -E */
ST_FUNC void tccgen_init(TCCState *s1)
{
    vtop = vstack - 1;
    memset(vtop, 0, sizeof *vtop);

    /* define some often used types */
    int_type.t = VT_INT;

    char_type.t = VT_BYTE;
    if (s1->char_is_unsigned)
        char_type.t |= VT_UNSIGNED;
    char_pointer_type = char_type;
    mk_pointer(&char_pointer_type);

    func_old_type.t = VT_FUNC;
    func_old_type.ref = sym_push(SYM_FIELD, &int_type, 0, 0);
    func_old_type.ref->f.func_call = FUNC_CDECL;
    func_old_type.ref->f.func_type = FUNC_OLD;
#ifdef precedence_parser
    init_prec();
#endif
    cstr_new(&initstr);
}

ST_FUNC int tccgen_compile(TCCState *s1)
{
    funcname = "";
    func_ind = -1;
    anon_sym = SYM_FIRST_ANOM;
    nocode_wanted = DATA_ONLY_WANTED; /* no code outside of functions */
    debug_modes = (s1->do_debug ? 1 : 0) | s1->test_coverage << 1;
    global_expr = 0;

    tcc_debug_start(s1);
    tcc_tcov_start (s1);
#ifdef TCC_TARGET_ARM
    arm_init(s1);
#endif
#ifdef INC_DEBUG
    printf("%s: **** new file\n", file->filename);
#endif
    parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | PARSE_FLAG_TOK_STR;
    next();
    decl(VT_CONST);
    gen_inline_functions(s1);
    method_compile_all();   /* 延迟编译方法体 (避免插入函数中途) */
    model_fn_compile_all();  /* 延迟编译 model function 实例 */
    check_vstack();
    /* end of translation unit info */
#if TCC_EH_FRAME
    tcc_eh_frame_end(s1);
#endif
    tcc_debug_end(s1);
    tcc_tcov_end(s1);
    return 0;
}

ST_FUNC void tccgen_finish(TCCState *s1)
{
    tcc_debug_end(s1); /* just in case of errors: free memory */
    free_inline_functions(s1);
    sym_pop(&global_stack, NULL, 0);
    sym_pop(&local_stack, NULL, 0);
    /* free preprocessor macros */
    free_defines(NULL);
    /* free sym_pools */
    dynarray_reset(&sym_pools, &nb_sym_pools);
    cstr_free(&initstr);
    dynarray_reset(&stk_data, &nb_stk_data);
    while (cur_switch)
        end_switch();
    local_scope = 0;
    loop_scope = NULL;
    all_cleanups = NULL;
    pending_gotos = NULL;
    nb_temp_local_vars = 0;
    global_label_stack = NULL;
    local_label_stack = NULL;
    cur_text_section = NULL;
    sym_free_first = NULL;
}

/* ------------------------------------------------------------------------- */
ST_FUNC ElfSym *elfsym(Sym *s)
{
  if (!s || !s->c)
    return NULL;
  return &((ElfSym *)symtab_section->data)[s->c];
}

/* apply storage attributes to Elf symbol */
ST_FUNC void update_storage(Sym *sym)
{
    ElfSym *esym;
    int sym_bind, old_sym_bind;

    esym = elfsym(sym);
    if (!esym)
        return;

    if (sym->a.visibility)
        esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1))
            | sym->a.visibility;

    if (sym->type.t & (VT_STATIC | VT_INLINE))
        sym_bind = STB_LOCAL;
    else if (sym->a.weak)
        sym_bind = STB_WEAK;
    else
        sym_bind = STB_GLOBAL;
    old_sym_bind = ELFW(ST_BIND)(esym->st_info);
    if (sym_bind != old_sym_bind) {
        esym->st_info = ELFW(ST_INFO)(sym_bind, ELFW(ST_TYPE)(esym->st_info));
    }

#ifdef TCC_TARGET_PE
    if (sym->a.dllimport)
        esym->st_other |= ST_PE_IMPORT;
    if (sym->a.dllexport)
        esym->st_other |= ST_PE_EXPORT;
#endif

#if 0
    printf("storage %s: bind=%c vis=%d exp=%d imp=%d\n",
        get_tok_str(sym->v, NULL),
        sym_bind == STB_WEAK ? 'w' : sym_bind == STB_LOCAL ? 'l' : 'g',
        sym->a.visibility,
        sym->a.dllexport,
        sym->a.dllimport
        );
#endif
}

/* ------------------------------------------------------------------------- */
/* update sym->c so that it points to an external symbol in section
   'section' with value 'value' */

ST_FUNC void put_extern_sym2(Sym *sym, int sh_num,
                            addr_t value, unsigned long size,
                            int can_add_underscore)
{
    int sym_type, sym_bind, info, other, t;
    ElfSym *esym;
    const char *name;
    char buf1[256];

    if (!sym->c) {
        name = get_tok_str(sym->v, NULL);
        t = sym->type.t;
        if ((t & VT_BTYPE) == VT_FUNC) {
            sym_type = STT_FUNC;
        } else if ((t & VT_BTYPE) == VT_VOID) {
            sym_type = STT_NOTYPE;
            if (IS_ASM_FUNC(t))
                sym_type = STT_FUNC;
        } else if (t & VT_TLS) {
            sym_type = STT_TLS;
        } else {
            sym_type = STT_OBJECT;
        }
        if (t & (VT_STATIC | VT_INLINE))
            sym_bind = STB_LOCAL;
        else
            sym_bind = STB_GLOBAL;
        other = 0;

#ifdef TCC_TARGET_PE
        if (sym_type == STT_FUNC && sym->type.ref) {
            Sym *ref = sym->type.ref;
            if (ref->a.nodecorate) {
                can_add_underscore = 0;
            }
            if (ref->f.func_call == FUNC_STDCALL && can_add_underscore) {
                sprintf(buf1, "_%s@%d", name, ref->f.func_args * PTR_SIZE);
                name = buf1;
                other |= ST_PE_STDCALL;
                can_add_underscore = 0;
            }
        }
#endif

        if (sym->asm_label) {
            name = get_tok_str(sym->asm_label, NULL);
            can_add_underscore = 0;
        }

        if (tcc_state->leading_underscore && can_add_underscore) {
            buf1[0] = '_';
            pstrcpy(buf1 + 1, sizeof(buf1) - 1, name);
            name = buf1;
        }

        info = ELFW(ST_INFO)(sym_bind, sym_type);
        sym->c = put_elf_sym(symtab_section, value, size, info, other, sh_num, name);

        if (debug_modes)
            tcc_debug_extern_sym(tcc_state, sym, sh_num, sym_bind, sym_type);

    } else {
        esym = elfsym(sym);
        esym->st_value = value;
        esym->st_size = size;
        esym->st_shndx = sh_num;
    }
    update_storage(sym);
}

ST_FUNC void put_extern_sym(Sym *sym, Section *s, addr_t value, unsigned long size)
{
    if (nocode_wanted && (NODATA_WANTED || (s && s == cur_text_section)))
        return;
    put_extern_sym2(sym, s ? s->sh_num : SHN_UNDEF, value, size, 1);
}

/* add a new relocation entry to symbol 'sym' in section 's' */
ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type,
                     addr_t addend)
{
    int c = 0;

    if (nocode_wanted && s == cur_text_section)
        return;

    if (sym) {
        if (0 == sym->c) {
            put_extern_sym(sym, NULL, 0, 0);
            if (sym->sym_scope
                && (sym->type.t & (VT_STATIC|VT_EXTERN)) == (VT_STATIC|VT_EXTERN)) {
                /* when a local function declaraion redeclares a global static one
                   then tccelf would not resolve them to the same symbol. */
                Sym *s = sym;
                while (s->prev_tok)
                    s = s->prev_tok;
                s->c = sym->c;
            }
        }
        c = sym->c;
    }

    /* now we can add ELF relocation info */
    put_elf_reloca(symtab_section, s, offset, type, c, addend);
}

#if PTR_SIZE == 4
ST_FUNC void greloc(Section *s, Sym *sym, unsigned long offset, int type)
{
    greloca(s, sym, offset, type, 0);
}
#endif

/* ------------------------------------------------------------------------- */
/* symbol allocator */
static Sym *__sym_malloc(void)
{
    Sym *sym_pool, *sym, *last_sym;
    int i;

    sym_pool = tcc_malloc(SYM_POOL_NB * sizeof(Sym));
    dynarray_add(&sym_pools, &nb_sym_pools, sym_pool);

    last_sym = sym_free_first;
    sym = sym_pool;
    for(i = 0; i < SYM_POOL_NB; i++) {
        sym->next = last_sym;
        last_sym = sym;
        sym++;
    }
    sym_free_first = last_sym;
    return last_sym;
}

static inline Sym *sym_malloc(void)
{
    Sym *sym;
#ifndef SYM_DEBUG
    sym = sym_free_first;
    if (!sym)
        sym = __sym_malloc();
    sym_free_first = sym->next;
    return sym;
#else
    sym = tcc_malloc(sizeof(Sym));
    return sym;
#endif
}

ST_INLN void sym_free(Sym *sym)
{
#ifndef SYM_DEBUG
    sym->next = sym_free_first;
    sym_free_first = sym;
#else
    tcc_free(sym);
#endif
}

/* push, without hashing */
ST_FUNC Sym *sym_push2(Sym **ps, int v, int t, int c)
{
    Sym *s;

    s = sym_malloc();
    memset(s, 0, sizeof *s);
    s->v = v;
    s->type.t = t;
    s->c = c;
    /* add in stack */
    s->prev = *ps;
    *ps = s;
    return s;
}

/* find a symbol and return its associated structure. 's' is the top
   of the symbol stack */
ST_FUNC Sym *sym_find2(Sym *s, int v)
{
    while (s) {
        if (s->v == v)
            return s;
        s = s->prev;
    }
    return NULL;
}

/* structure lookup */
ST_INLN Sym *struct_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_struct;
}

/* find an identifier */
ST_INLN Sym *sym_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_identifier;
}

/* make sym in-/visible to the parser */
static inline void sym_link(Sym *s, int yes)
{
    TokenSym *ts = table_ident[(s->v & ~SYM_STRUCT) - TOK_IDENT];
    Sym **ps;
    if (s->v & SYM_STRUCT)
        ps = &ts->sym_struct;
    else
        ps = &ts->sym_identifier;
    if (yes) {
        s->prev_tok = *ps, *ps = s;
        s->sym_scope = local_scope;
    } else {
        *ps = s->prev_tok;
    }
}

static inline int sym_scope_ex(Sym *s)
{
    /* enums have 'sym_scope' overwritten by 'enum_val' */
    return IS_ENUM_VAL (s->type.t)
        ? s->type.ref->sym_scope
        : s->sym_scope;
}

/* push a given symbol on the symbol stack */
ST_FUNC Sym *sym_push(int v, CType *type, int r, int c)
{
    Sym *s, **ps;
    if (local_stack)
        ps = &local_stack;
    else
        ps = &global_stack;
    s = sym_push2(ps, v, type->t, c);
    s->type.ref = type->ref;
    s->r = r;
    /* don't record fields or anonymous symbols */
    if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
        /* record symbol in token array */
        sym_link(s, 1);
        if (s->prev_tok && sym_scope_ex(s->prev_tok) == local_scope)
            tcc_error("redeclaration of '%s'", get_tok_str(s->v, NULL));
    }
    return s;
}

/* push a global identifier */
ST_FUNC Sym *global_identifier_push(int v, int t, int c)
{
    Sym *s, **ps;
    s = sym_push2(&global_stack, v, t, c);
    s->r = VT_CONST | VT_SYM;
    /* don't record anonymous symbol */
    if (v < SYM_FIRST_ANOM) {
        ps = &table_ident[v - TOK_IDENT]->sym_identifier;
        /* modify the top most local identifier, so that sym_identifier will
           point to 's' when popped; happens when called from inline asm */
        while (*ps != NULL && (*ps)->sym_scope)
            ps = &(*ps)->prev_tok;
        s->prev_tok = *ps;
        *ps = s;
    }
    return s;
}

/* pop symbols until top reaches 'b'.  If KEEP is non-zero don't really
   pop them yet from the list, but do remove them from the token array.  */
ST_FUNC void sym_pop(Sym **ptop, Sym *b, int keep)
{
    Sym *s, *ss;
    int v;

    s = *ptop;
    while(s != b) {
        ss = s->prev;
        v = s->v;
        /* remove symbol in token array */
        if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
            sym_link(s, 0);
	if (!keep)
	    sym_free(s);
        s = ss;
    }
    if (!keep)
	*ptop = b;
}

/* label lookup */
ST_FUNC Sym *label_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_label;
}

ST_FUNC Sym *label_push(Sym **ptop, int v, int flags)
{
    Sym *s, **ps;
    s = sym_push2(ptop, v, VT_STATIC, 0);
    s->r = flags;
    ps = &table_ident[v - TOK_IDENT]->sym_label;
    if (ptop == &global_label_stack) {
        /* modify the top most local identifier, so that
           sym_identifier will point to 's' when popped */
        while (*ps != NULL)
            ps = &(*ps)->prev_tok;
    }
    s->prev_tok = *ps;
    *ps = s;
    return s;
}

/* pop labels until element last is reached. Look if any labels are
   undefined. Define symbols if '&&label' was used. */
ST_FUNC void label_pop(Sym **ptop, Sym *slast, int keep)
{
    Sym *s, *s1;
    for(s = *ptop; s != slast; s = s1) {
        s1 = s->prev;
        if (s->r == LABEL_DECLARED) {
            tcc_warning_c(warn_all)("label '%s' declared but not used", get_tok_str(s->v, NULL));
        } else if (s->r == LABEL_FORWARD) {
                tcc_error("label '%s' used but not defined",
                      get_tok_str(s->v, NULL));
        } else {
            if (s->c) {
                /* define corresponding symbol. A size of
                   1 is put. */
                put_extern_sym(s, cur_text_section, s->jnext, 1);
            }
        }
        /* remove label */
        if (s->r != LABEL_GONE)
            table_ident[s->v - TOK_IDENT]->sym_label = s->prev_tok;
        if (!keep)
            sym_free(s);
        else
            s->r = LABEL_GONE;
    }
    if (!keep)
        *ptop = slast;
}

/* ------------------------------------------------------------------------- */
static void vcheck_cmp(void)
{
    /* cannot let cpu flags if other instruction are generated. Also
       avoid leaving VT_JMP anywhere except on the top of the stack
       because it would complicate the code generator.

       Don't do this when nocode_wanted.  vtop might come from
       !nocode_wanted regions (see 88_codeopt.c) and transforming
       it to a register without actually generating code is wrong
       as their value might still be used for real.  All values
       we push under nocode_wanted will eventually be popped
       again, so that the VT_CMP/VT_JMP value will be in vtop
       when code is unsuppressed again. */

    /* However if it's just automatic suppression via CODE_OFF/ON()
       then it seems that we better let things work undisturbed.
       How can it work at all under nocode_wanted?  Well, gv() will
       actually clear it at the gsym() in load()/VT_JMP in the
       generator backends */

    if (vtop->r == VT_CMP && 0 == (nocode_wanted & ~CODE_OFF_BIT))
        gv(RC_INT);
}

static void vsetc(CType *type, int r, CValue *vc)
{
    if (vtop >= vstack + (VSTACK_SIZE - 1))
        tcc_error("memory full (vstack)");
    vcheck_cmp();
    vtop++;
    vtop->type = *type;
    vtop->r = r;
    vtop->r2 = VT_CONST;
    vtop->c = *vc;
    vtop->sym = NULL;
}

ST_FUNC void vswap(void)
{
    SValue tmp;

    vcheck_cmp();
    tmp = vtop[0];
    vtop[0] = vtop[-1];
    vtop[-1] = tmp;
}

/* pop stack value */
ST_FUNC void vpop(void)
{
    int v;
    v = vtop->r & VT_VALMASK;
#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_X86_64)
    /* for x86, we need to pop the FP stack */
    if (v == TREG_ST0) {
        o(0xd8dd); /* fstp %st(0) */
    } else
#endif
    if (v == VT_CMP) {
        /* need to put correct jump if && or || without test */
        gsym(vtop->jtrue);
        gsym(vtop->jfalse);
    }
    vtop--;
}

/* push constant of type "type" with useless value */
static void vpush(CType *type)
{
    vset(type, VT_CONST, 0);
}

/* push arbitrary 64bit constant */
static void vpush64(int ty, unsigned long long v)
{
    CValue cval;
    CType ctype;
    ctype.t = ty;
    ctype.ref = NULL;
    cval.i = v;
    vsetc(&ctype, VT_CONST, &cval);
}

/* push integer constant */
ST_FUNC void vpushi(int v)
{
    vpush64(VT_INT, v);
}

/* push a pointer sized constant */
static void vpushs(addr_t v)
{
    vpush64(VT_SIZE_T, v);
}

/* push long long constant */
static inline void vpushll(long long v)
{
    vpush64(VT_LLONG, v);
}

ST_FUNC void vset(CType *type, int r, int v)
{
    CValue cval;
    cval.i = v;
    vsetc(type, r, &cval);
}

static void vseti(int r, int v)
{
    CType type;
    type.t = VT_INT;
    type.ref = NULL;
    vset(&type, r, v);
}

ST_FUNC void vpushv(SValue *v)
{
    if (vtop >= vstack + (VSTACK_SIZE - 1))
        tcc_error("memory full (vstack)");
    vtop++;
    *vtop = *v;
}

static void vdup(void)
{
    vpushv(vtop);
}

/* rotate the stack element at position n-1 to the top */
ST_FUNC void vrotb(int n)
{
    SValue tmp;
    if (--n < 1)
        return;
    vcheck_cmp();
    tmp = vtop[-n];
    memmove(vtop - n, vtop - n + 1, sizeof *vtop * n);
    vtop[0] = tmp;
}

/* rotate the top stack element into position n-1 */
ST_FUNC void vrott(int n)
{
    SValue tmp;
    if (--n < 1)
        return;
    vcheck_cmp();
    tmp = vtop[0];
    memmove(vtop - n + 1, vtop - n, sizeof *vtop * n);
    vtop[-n] = tmp;
}

/* reverse order of the the first n stack elements */
ST_FUNC void vrev(int n)
{
    int i;
    SValue tmp;
    vcheck_cmp();
    for (i = 0, n = -n; i > ++n; --i)
        tmp = vtop[i], vtop[i] = vtop[n], vtop[n] = tmp;
}

/* ------------------------------------------------------------------------- */
/* vtop->r = VT_CMP means CPU-flags have been set from comparison or test. */

/* called from generators to set the result from relational ops  */
ST_FUNC void vset_VT_CMP(int op)
{
    vtop->r = VT_CMP;
    vtop->cmp_op = op;
    vtop->jfalse = 0;
    vtop->jtrue = 0;
}

/* called once before asking generators to load VT_CMP to a register */
static void vset_VT_JMP(void)
{
    int op = vtop->cmp_op;

    if (vtop->jtrue || vtop->jfalse) {
        int origt = vtop->type.t;
        /* we need to jump to 'mov $0,%R' or 'mov $1,%R' */
        int inv = op & (op < 2); /* small optimization */
        vseti(VT_JMP+inv, gvtst(inv, 0));
        vtop->type.t |= origt & (VT_UNSIGNED | VT_DEFSIGN);
    } else {
        /* otherwise convert flags (rsp. 0/1) to register */
        vtop->c.i = op;
        if (op < 2) /* doesn't seem to happen */
            vtop->r = VT_CONST;
    }
}

/* Set CPU Flags, doesn't yet jump */
static void gvtst_set(int inv, int t)
{
    int *p;

    if (vtop->r != VT_CMP) {
        vpushi(0);
        gen_op(TOK_NE);
        if (vtop->r != VT_CMP) /* must be VT_CONST then */
            vset_VT_CMP(vtop->c.i != 0);
    }

    p = inv ? &vtop->jfalse : &vtop->jtrue;
    *p = gjmp_append(*p, t);
}

/* Generate value test
 *
 * Generate a test for any value (jump, comparison and integers) */
static int gvtst(int inv, int t)
{
    int op, x, u;

    gvtst_set(inv, t);
    t = vtop->jtrue, u = vtop->jfalse;
    if (inv)
        x = u, u = t, t = x;
    op = vtop->cmp_op;

    /* jump to the wanted target */
    if (op > 1)
        t = gjmp_cond(op ^ inv, t);
    else if (op != inv)
        t = gjmp(t);
    /* resolve complementary jumps to here */
    gsym(u);

    vtop--;
    return t;
}

/* generate a zero or nozero test */
static void gen_test_zero(int op)
{
    if (vtop->r == VT_CMP) {
        int j;
        if (op == TOK_EQ) {
            j = vtop->jfalse;
            vtop->jfalse = vtop->jtrue;
            vtop->jtrue = j;
            vtop->cmp_op ^= 1;
        }
    } else {
        vpushi(0);
        gen_op(op);
    }
}

/* ------------------------------------------------------------------------- */
/* push a symbol value of TYPE */
ST_FUNC void vpushsym(CType *type, Sym *sym)
{
    CValue cval;
    cval.i = 0;
    vsetc(type, VT_CONST | VT_SYM, &cval);
    vtop->sym = sym;
}

/* Return a static symbol pointing to a section */
ST_FUNC Sym *get_sym_ref(CType *type, Section *sec, unsigned long offset, unsigned long size)
{
    int v;
    Sym *sym;

    v = anon_sym++;
    sym = sym_push(v, type, VT_CONST | VT_SYM, 0);
    sym->type.t |= VT_STATIC;
    put_extern_sym(sym, sec, offset, size);
    return sym;
}

/* push a reference to a section offset by adding a dummy symbol */
static void vpush_ref(CType *type, Section *sec, unsigned long offset, unsigned long size)
{
    vpushsym(type, get_sym_ref(type, sec, offset, size));  
}

/* define a new external reference to a symbol 'v' of type 'u' */
ST_FUNC Sym *external_global_sym(int v, CType *type)
{
    Sym *s;

    s = sym_find(v);
    if (!s) {
        /* push forward reference */
        s = global_identifier_push(v, type->t | VT_EXTERN, 0);
        s->type.ref = type->ref;
    } else if (IS_ASM_SYM(s)) {
        s->type.t = type->t | (s->type.t & VT_EXTERN);
        s->type.ref = type->ref;
        update_storage(s);
    }
    return s;
}

/* create an external reference with no specific type similar to asm labels.
   This avoids type conflicts if the symbol is used from C too */
ST_FUNC Sym *external_helper_sym(int v)
{
    CType ct = { VT_ASM_FUNC, NULL };
    return external_global_sym(v, &ct);
}

/* push a reference to an helper function (such as memmove) */
ST_FUNC void vpush_helper_func(int v)
{
    vpushsym(&func_old_type, external_helper_sym(v));
}

/* Merge symbol attributes.  */
static void merge_symattr(struct SymAttr *sa, struct SymAttr *sa1)
{
    if (sa1->aligned && !sa->aligned)
      sa->aligned = sa1->aligned;
    sa->packed |= sa1->packed;
    sa->weak |= sa1->weak;
    sa->nodebug |= sa1->nodebug;
    if (sa1->visibility != STV_DEFAULT) {
	int vis = sa->visibility;
	if (vis == STV_DEFAULT
	    || vis > sa1->visibility)
	  vis = sa1->visibility;
	sa->visibility = vis;
    }
    sa->dllexport |= sa1->dllexport;
    sa->nodecorate |= sa1->nodecorate;
    sa->dllimport |= sa1->dllimport;
}

/* Merge function attributes.  */
static void merge_funcattr(struct FuncAttr *fa, struct FuncAttr *fa1)
{
    if (fa1->func_call && !fa->func_call)
      fa->func_call = fa1->func_call;
    if (fa1->func_type && !fa->func_type)
      fa->func_type = fa1->func_type;
    if (fa1->func_args && !fa->func_args)
      fa->func_args = fa1->func_args;
    if (fa1->func_noreturn)
      fa->func_noreturn = 1;
    if (fa1->func_ctor)
      fa->func_ctor = 1;
    if (fa1->func_dtor)
      fa->func_dtor = 1;
}

/* Merge attributes.  */
static void merge_attr(AttributeDef *ad, AttributeDef *ad1)
{
    merge_symattr(&ad->a, &ad1->a);
    merge_funcattr(&ad->f, &ad1->f);

    if (ad1->section)
      ad->section = ad1->section;
    if (ad1->alias_target)
      ad->alias_target = ad1->alias_target;
    if (ad1->asm_label)
      ad->asm_label = ad1->asm_label;
    if (ad1->attr_mode)
      ad->attr_mode = ad1->attr_mode;
}

/* Merge some type attributes.  */
static void patch_type(Sym *sym, CType *type)
{
    if (!(type->t & VT_EXTERN) || IS_ENUM_VAL(sym->type.t)) {
        if (!(sym->type.t & VT_EXTERN))
            tcc_error("redefinition of '%s'", get_tok_str(sym->v, NULL));
        sym->type.t &= ~VT_EXTERN;
    }

    if (IS_ASM_SYM(sym)) {
        /* stay static if both are static */
        sym->type.t = type->t & (sym->type.t | ~VT_STATIC);
        sym->type.ref = type->ref;
        if ((type->t & VT_BTYPE) != VT_FUNC && !(type->t & VT_ARRAY))
            sym->r |= VT_LVAL;
    }

    if (!is_compatible_types(&sym->type, type)) {
        tcc_error("incompatible types for redefinition of '%s'",
                  get_tok_str(sym->v, NULL));

    } else if ((sym->type.t & VT_BTYPE) == VT_FUNC) {
        int static_proto = sym->type.t & VT_STATIC;
        int ft1 = sym->type.ref->f.func_type;
        int ft2 = type->ref->f.func_type;

        /* warn if static follows non-static function declaration */
        if ((type->t & VT_STATIC) && !static_proto
            /* XXX this test for inline shouldn't be here.  Until we
               implement gnu-inline mode again it silences a warning for
               mingw caused by our workarounds.  */
            && !((type->t | sym->type.t) & VT_INLINE))
            tcc_warning("static storage ignored for redefinition of '%s'",
                get_tok_str(sym->v, NULL));

        /* set 'inline' if both agree or if one has static */
        if ((type->t | sym->type.t) & VT_INLINE) {
            if (!((type->t ^ sym->type.t) & VT_INLINE)
             || ((type->t | sym->type.t) & VT_STATIC))
                static_proto |= VT_INLINE;
        }

        if (0 == (type->t & VT_EXTERN)) {
            struct FuncAttr f = sym->type.ref->f;
            /* put complete type, use static from prototype */
            sym->type.t = (type->t & ~(VT_STATIC|VT_INLINE)) | static_proto;
            if (ft1 != FUNC_OLD)
                type->ref->f.func_type = ft1;
            sym->type.ref = type->ref;
            merge_funcattr(&sym->type.ref->f, &f);
        } else {
            sym->type.t &= ~VT_INLINE | static_proto;
            if (ft1 == FUNC_OLD && ft2 != FUNC_OLD)
                sym->type.ref = type->ref;
        }

    } else {
        if ((sym->type.t & VT_ARRAY) && type->ref->c >= 0) {
            /* set array size if it was omitted in extern declaration */
            sym->type.ref->c = type->ref->c;
        }
        if ((type->t ^ sym->type.t) & VT_STATIC)
            tcc_warning("storage mismatch for redefinition of '%s'",
                get_tok_str(sym->v, NULL));
    }
}

/* Merge some storage attributes.  */
static void patch_storage(Sym *sym, AttributeDef *ad, CType *type)
{
    if (type)
        patch_type(sym, type);

#ifdef TCC_TARGET_PE
    if (sym->a.dllimport != ad->a.dllimport)
        tcc_error("incompatible dll linkage for redefinition of '%s'",
            get_tok_str(sym->v, NULL));
#endif
    merge_symattr(&sym->a, &ad->a);
    if (ad->asm_label)
        sym->asm_label = ad->asm_label;
    update_storage(sym);
}

/* copy sym to other stack */
static Sym *sym_copy(Sym *s0, Sym **ps)
{
    Sym *s;
    s = sym_malloc(), *s = *s0;
    s->prev = *ps, *ps = s;
    if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
        sym_link(s, 1);
    return s;
}

/* Symbol 's' was locally declared 'extern' (or as function), and is
   on global_stack.  Now must copy its 'ref' to global_stack too */
static void move_ref_to_global(Sym *s)
{
    Sym *l, **lp;
    int n, bt;

    bt = s->type.t & VT_BTYPE;
    if (!(bt == VT_PTR
       || bt == VT_FUNC
       || bt == VT_STRUCT
       || IS_ENUM(s->type.t)))
        return;

    for (s = s->type.ref, n = 0; s; s = s->next) {
        for (lp = &local_stack; !!(l = *lp); lp = &l->prev) {
            if (l == s) {
                *lp = s->prev;
                s->prev = global_stack, global_stack = s;
                if (n || bt == VT_PTR || bt == VT_FUNC) {
                    move_ref_to_global(s);
                } else {
                    if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
                        /* copy struct/enum tag to local scope */
                        s->v |= SYM_FIELD;
                        l = sym_copy(s, lp);
                        l->v &= ~SYM_FIELD;
                    }
                }
                if (bt != VT_PTR)
                    n = 1;
                break;
            }
        }
        if (n == 0) /* no next (VT_PTR) or ref not on local_stack */
            break;
    }
}

/* define a new external reference to a symbol 'v' */
static Sym *external_sym(int v, CType *type, int r, AttributeDef *ad)
{
    Sym *s;

    /* look for global symbol */
    s = sym_find(v);
    while (s && s->sym_scope)
        s = s->prev_tok;

    if (!s) {
        /* push forward reference */
        s = global_identifier_push(v, type->t, 0);
        s->r |= r;
        s->a = ad->a;
        s->asm_label = ad->asm_label;
        s->type.ref = type->ref;
    } else {
        patch_storage(s, ad, type);
    }
    if (local_stack) {
        /* make sure that type->ref is on global stack */
        move_ref_to_global(s);
        /* put into local scope */
        sym_copy(s, &local_stack);
    }
    return s;
}

/* save registers up to (vtop - n) stack entry */
ST_FUNC void save_regs(int n)
{
    SValue *p, *p1;
    for(p = vstack, p1 = vtop - n; p <= p1; p++)
        save_reg(p->r);
}

/* save r to the memory stack, and mark it as being free */
ST_FUNC void save_reg(int r)
{
    save_reg_upstack(r, 0);
}

/* save r to the memory stack, and mark it as being free,
   if seen up to (vtop - n) stack entry */
ST_FUNC void save_reg_upstack(int r, int n)
{
    int l, size, align, bt, r2;
    SValue *p, *p1, sv;

    if ((r &= VT_VALMASK) >= VT_CONST)
        return;
    if (nocode_wanted)
        return;
    l = r2 = 0;
    for(p = vstack, p1 = vtop - n; p <= p1; p++) {
        if ((p->r & VT_VALMASK) == r || p->r2 == r) {
            /* must save value on stack if not already done */
            if (!l) {
                bt = p->type.t & VT_BTYPE;
                if (bt == VT_VOID)
                    continue;
                if ((p->r & VT_LVAL) || bt == VT_FUNC)
                    bt = VT_PTR;
                sv.type.t = bt;
                size = type_size(&sv.type, &align);
                l = get_temp_local_var(size, align, &r2);
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.i = l;
		sv.sym = NULL;
                store(p->r & VT_VALMASK, &sv);
#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_X86_64)
                /* x86 specific: need to pop fp register ST0 if saved */
                if (r == TREG_ST0) {
                    o(0xd8dd); /* fstp %st(0) */
                }
#endif
                /* special long long case */
                if (p->r2 < VT_CONST && USING_TWO_WORDS(bt)) {
                    sv.c.i += PTR_SIZE;
                    store(p->r2, &sv);
                }
            }
            /* mark that stack entry as being saved on the stack */
            if (p->r & VT_LVAL) {
                /* also clear the bounded flag because the
                   relocation address of the function was stored in
                   p->c.i */
                p->r = (p->r & ~(VT_VALMASK | VT_BOUNDED)) | VT_LLOCAL;
            } else {
                p->r = VT_LVAL | VT_LOCAL;
                p->type.t &= ~VT_ARRAY; /* cannot combine VT_LVAL with VT_ARRAY */
            }
            p->sym = NULL;
            p->r2 = r2;
            p->c.i = l;
        }
    }
}

#ifdef TCC_TARGET_ARM
/* find a register of class 'rc2' with at most one reference on stack.
 * If none, call get_reg(rc) */
ST_FUNC int get_reg_ex(int rc, int rc2)
{
    int r;
    SValue *p;
    
    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc2) {
            int n;
            n=0;
            for(p = vstack; p <= vtop; p++) {
                if ((p->r & VT_VALMASK) == r ||
                    p->r2 == r)
                    n++;
            }
            if (n <= 1)
                return r;
        }
    }
    return get_reg(rc);
}
#endif

/* find a free register of class 'rc'. If none, save one register */
ST_FUNC int get_reg(int rc)
{
    int r;
    SValue *p;

    /* find a free register */
    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc) {
            if (nocode_wanted)
                return r;
            for(p=vstack;p<=vtop;p++) {
                if ((p->r & VT_VALMASK) == r ||
                    p->r2 == r)
                    goto notfound;
            }
            return r;
        }
    notfound: ;
    }
    
    /* no register left : free the first one on the stack (VERY
       IMPORTANT to start from the bottom to ensure that we don't
       spill registers used in gen_opi()) */
    for(p=vstack;p<=vtop;p++) {
        /* look at second register (if long long) */
        r = p->r2;
        if (r < VT_CONST && (reg_classes[r] & rc))
            goto save_found;
        r = p->r & VT_VALMASK;
        if (r < VT_CONST && (reg_classes[r] & rc)) {
        save_found:
            save_reg(r);
            return r;
        }
    }
    /* Should never comes here */
    return -1;
}

/* find a free temporary local variable (return the offset on stack) match
   size and align. If none, add new temporary stack variable */
static int get_temp_local_var(int size,int align, int *r2)
{
    int i;
    struct temp_local_variable *temp_var;
    SValue *p;
    int r;
    unsigned used = 0;

    /* mark locations that are still in use */
    for (p = vstack; p <= vtop; p++) {
	r = p->r & VT_VALMASK;
	if (r == VT_LOCAL || r == VT_LLOCAL) {
	    r = p->r2 - (VT_CONST + 1);
	    if (r >= 0 && r < MAX_TEMP_LOCAL_VARIABLE_NUMBER)
	        used |= 1<<r;
	}
    }
    for (i=0;i<nb_temp_local_vars;i++) {
	temp_var=&arr_temp_local_vars[i];
	if(!(used & 1<<i)
	 && temp_var->size>=size
	 && temp_var->align>=align) {
ret_tmp:
	    *r2 = (VT_CONST + 1) + i;
	    return temp_var->location;
	}
    }
    loc = (loc - size) & -align;
    if (nb_temp_local_vars<MAX_TEMP_LOCAL_VARIABLE_NUMBER) {
	temp_var=&arr_temp_local_vars[i];
	temp_var->location=loc;
	temp_var->size=size;
	temp_var->align=align;
	nb_temp_local_vars++;
	goto ret_tmp;
    }
    *r2 = VT_CONST;
    return loc;
}

/* move register 's' (of type 't') to 'r', and flush previous value of r to memory
   if needed */
static void move_reg(int r, int s, int t)
{
    SValue sv;

    if (r != s) {
        save_reg(r);
        sv.type.t = t;
        sv.type.ref = NULL;
        sv.r = s;
        sv.c.i = 0;
        load(r, &sv);
    }
}

/* get address of vtop (vtop MUST BE an lvalue) */
ST_FUNC void gaddrof(void)
{
    vtop->r &= ~VT_LVAL;
    /* tricky: if saved lvalue, then we can go back to lvalue */
    if ((vtop->r & VT_VALMASK) == VT_LLOCAL)
        vtop->r = (vtop->r & ~VT_VALMASK) | VT_LOCAL | VT_LVAL;
}

#ifdef CONFIG_TCC_BCHECK
/* generate a bounded pointer addition */
static void gen_bounded_ptr_add(void)
{
    int save = (vtop[-1].r & VT_VALMASK) == VT_LOCAL;
    if (save) {
      vpushv(&vtop[-1]);
      vrott(3);
    }
    vpush_helper_func(TOK___bound_ptr_add);
    vrott(3);
    gfunc_call(2);
    vtop -= save;
    vpushi(0);
    /* returned pointer is in REG_IRET */
    vtop->r = REG_IRET | VT_BOUNDED;
    if (nocode_wanted)
        return;
    /* relocation offset of the bounding function call point */
    vtop->c.i = (cur_text_section->reloc->data_offset - sizeof(ElfW_Rel));
}

/* patch pointer addition in vtop so that pointer dereferencing is
   also tested */
static void gen_bounded_ptr_deref(void)
{
    addr_t func;
    int size, align;
    ElfW_Rel *rel;
    Sym *sym;

    if (nocode_wanted)
        return;

    size = type_size(&vtop->type, &align);
    switch(size) {
    case  1: func = TOK___bound_ptr_indir1; break;
    case  2: func = TOK___bound_ptr_indir2; break;
    case  4: func = TOK___bound_ptr_indir4; break;
    case  8: func = TOK___bound_ptr_indir8; break;
    case 12: func = TOK___bound_ptr_indir12; break;
    case 16: func = TOK___bound_ptr_indir16; break;
    default:
        /* may happen with struct member access */
        return;
    }
    sym = external_helper_sym(func);
    if (!sym->c)
        put_extern_sym(sym, NULL, 0, 0);
    /* patch relocation */
    /* XXX: find a better solution ? */
    rel = (ElfW_Rel *)(cur_text_section->reloc->data + vtop->c.i);
    rel->r_info = ELFW(R_INFO)(sym->c, ELFW(R_TYPE)(rel->r_info));
}

/* generate lvalue bound code */
static void gbound(void)
{
    CType type1;

    vtop->r &= ~VT_MUSTBOUND;
    /* if lvalue, then use checking code before dereferencing */
    if (vtop->r & VT_LVAL) {
        /* if not VT_BOUNDED value, then make one */
        if (!(vtop->r & VT_BOUNDED)) {
            /* must save type because we must set it to int to get pointer */
            type1 = vtop->type;
            vtop->type.t = VT_PTR;
            gaddrof();
            vpushi(0);
            gen_bounded_ptr_add();
            vtop->r |= VT_LVAL;
            vtop->type = type1;
        }
        /* then check for dereferencing */
        gen_bounded_ptr_deref();
    }
}

/* we need to call __bound_ptr_add before we start to load function
   args into registers */
ST_FUNC void gbound_args(int nb_args)
{
    int i, v;
    SValue *sv;

    for (i = 1; i <= nb_args; ++i)
        if (vtop[1 - i].r & VT_MUSTBOUND) {
            vrotb(i);
            gbound();
            vrott(i);
        }

    sv = vtop - nb_args;
    if (sv->r & VT_SYM) {
        v = sv->sym->v;
        if (v == TOK_setjmp
          || v == TOK__setjmp
#ifndef TCC_TARGET_PE
          || v == TOK_sigsetjmp
          || v == TOK___sigsetjmp
#endif
          ) {
            vpush_helper_func(TOK___bound_setjmp);
            vpushv(sv + 1);
            gfunc_call(1);
            func_bound_add_epilog = 1;
        }
        if (v == TOK_alloca)
            func_bound_add_epilog = 1;
#if TARGETOS_NetBSD
        if (v == TOK_longjmp) /* undo rename to __longjmp14 */
            sv->sym->asm_label = TOK___bound_longjmp;
#endif
    }
}

/* Add bounds for local symbols from S to E (via ->prev) */
static void add_local_bounds(Sym *s, Sym *e)
{
    for (; s != e; s = s->prev) {
        if (!s->v || (s->r & VT_VALMASK) != VT_LOCAL)
          continue;
        /* Add arrays/structs/unions because we always take address */
        if ((s->type.t & VT_ARRAY)
            || (s->type.t & VT_BTYPE) == VT_STRUCT
            || s->a.addrtaken) {
            /* add local bound info */
            int align, size = type_size(&s->type, &align);
            addr_t *bounds_ptr = section_ptr_add(lbounds_section,
                                                 2 * sizeof(addr_t));
            bounds_ptr[0] = s->c;
            bounds_ptr[1] = size;
        }
    }
}
#endif

/* add debug info for locals or function parameters, optionally
   register bounds */
static void tcc_debug_end_scope(Sym *b, int bounds)
{
#ifdef CONFIG_TCC_BCHECK
    if (tcc_state->do_bounds_check && bounds)
        add_local_bounds(local_stack, b);
#endif
    tcc_add_debug_info (tcc_state, local_stack, b);
}

/* increment an lvalue pointer */
static void incr_offset(int offset)
{
    int t = vtop->type.t;
    gaddrof(); /* remove VT_LVAL */
    vtop->type.t = VT_PTRDIFF_T; /* set scalar type */
    vpushs(offset);
    gen_op('+');
    vtop->r |= VT_LVAL;
    vtop->type.t = t;
}

static void incr_bf_adr(int o)
{
    vtop->type.t = VT_BYTE | VT_UNSIGNED;
    incr_offset(o);
}

/* single-byte load mode for packed or otherwise unaligned bitfields */
static void load_packed_bf(CType *type, int bit_pos, int bit_size)
{
    int n, o, bits;
    save_reg_upstack(vtop->r, 1);
    vpush64(type->t & VT_BTYPE, 0); // B X
    bits = 0, o = bit_pos >> 3, bit_pos &= 7;
    do {
        vswap(); // X B
        incr_bf_adr(o);
        vdup(); // X B B
        n = 8 - bit_pos;
        if (n > bit_size)
            n = bit_size;
        if (bit_pos)
            vpushi(bit_pos), gen_op(TOK_SHR), bit_pos = 0; // X B Y
        if (n < 8)
            vpushi((1 << n) - 1), gen_op('&');
        gen_cast(type);
        if (bits)
            vpushi(bits), gen_op(TOK_SHL);
        vrotb(3); // B Y X
        gen_op('|'); // B X
        bits += n, bit_size -= n, o = 1;
    } while (bit_size);
    vswap(), vpop();
    if (!(type->t & VT_UNSIGNED)) {
        n = ((type->t & VT_BTYPE) == VT_LLONG ? 64 : 32) - bits;
        vpushi(n), gen_op(TOK_SHL);
        vpushi(n), gen_op(TOK_SAR);
    }
}

/* single-byte store mode for packed or otherwise unaligned bitfields */
static void store_packed_bf(int bit_pos, int bit_size)
{
    int bits, n, o, m, c;
    c = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    vswap(); // X B
    save_reg_upstack(vtop->r, 1);
    bits = 0, o = bit_pos >> 3, bit_pos &= 7;
    do {
        incr_bf_adr(o); // X B
        vswap(); //B X
        c ? vdup() : gv_dup(); // B V X
        vrott(3); // X B V
        if (bits)
            vpushi(bits), gen_op(TOK_SHR);
        if (bit_pos)
            vpushi(bit_pos), gen_op(TOK_SHL);
        n = 8 - bit_pos;
        if (n > bit_size)
            n = bit_size;
        if (n < 8) {
            m = ((1 << n) - 1) << bit_pos;
            vpushi(m), gen_op('&'); // X B V1
            vpushv(vtop-1); // X B V1 B
            vpushi(m & 0x80 ? ~m & 0x7f : ~m);
            gen_op('&'); // X B V1 B1
            gen_op('|'); // X B V2
        }
        vdup(), vtop[-1] = vtop[-2]; // X B B V2
        vstore(), vpop(); // X B
        bits += n, bit_size -= n, bit_pos = 0, o = 1;
    } while (bit_size);
    vpop(), vpop();
}

static int adjust_bf(SValue *sv, int bit_pos, int bit_size)
{
    int t;
    if (0 == sv->type.ref)
        return 0;
    t = sv->type.ref->auxtype;
    if (t != -1 && t != VT_STRUCT) {
        sv->type.t = (sv->type.t & ~(VT_BTYPE | VT_LONG)) | t;
        sv->r |= VT_LVAL;
    }
    return t;
}

/* store vtop a register belonging to class 'rc'. lvalues are
   converted to values. Cannot be used if cannot be converted to
   register value (such as structures). */
ST_FUNC int gv(int rc)
{
    int r, r2, r_ok, r2_ok, rc2, bt;
    int bit_pos, bit_size, size, align;

    /* NOTE: get_reg can modify vstack[] */
    if (vtop->type.t & VT_BITFIELD) {
        CType type;

        bit_pos = BIT_POS(vtop->type.t);
        bit_size = BIT_SIZE(vtop->type.t);
        /* remove bit field info to avoid loops */
        vtop->type.t &= ~VT_STRUCT_MASK;

        type.ref = NULL;
        type.t = vtop->type.t & VT_UNSIGNED;
        if ((vtop->type.t & VT_BTYPE) == VT_BOOL)
            type.t |= VT_UNSIGNED;

        r = adjust_bf(vtop, bit_pos, bit_size);

        if ((vtop->type.t & VT_BTYPE) == VT_LLONG)
            type.t |= VT_LLONG;
        else
            type.t |= VT_INT;

        if (r == VT_STRUCT) {
            load_packed_bf(&type, bit_pos, bit_size);
        } else {
            int bits = (type.t & VT_BTYPE) == VT_LLONG ? 64 : 32;
            /* cast to int to propagate signedness in following ops */
            gen_cast(&type);
            /* generate shifts */
            vpushi(bits - (bit_pos + bit_size));
            gen_op(TOK_SHL);
            vpushi(bits - bit_size);
            /* NOTE: transformed to SHR if unsigned */
            gen_op(TOK_SAR);
        }
        r = gv(rc);
    } else {
        if (is_float(vtop->type.t) && 
            (vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
            /* CPUs usually cannot use float constants, so we store them
               generically in data segment */
            init_params p = { rodata_section };
            unsigned long offset;
            size = type_size(&vtop->type, &align);
            if (NODATA_WANTED)
                size = 0, align = 1;
            offset = section_add(p.sec, size, align);
            vpush_ref(&vtop->type, p.sec, offset, size);
	    vswap();
	    init_putv(&p, &vtop->type, offset);
	    vtop->r |= VT_LVAL;
        }
#ifdef CONFIG_TCC_BCHECK
        if (vtop->r & VT_MUSTBOUND) 
            gbound();
#endif

        bt = vtop->type.t & VT_BTYPE;
        if (bt == VT_VOID || bt == VT_STRUCT) /* should not happen */
            return vtop->r;

#ifdef TCC_TARGET_RISCV64
        /* XXX mega hack */
        if (bt == VT_LDOUBLE && rc == RC_FLOAT)
          rc = RC_INT;
#endif
        rc2 = RC2_TYPE(bt, rc);

        /* need to reload if:
           - constant
           - lvalue (need to dereference pointer)
           - already a register, but not in the right class */
        r = vtop->r & VT_VALMASK;
        r_ok = !(vtop->r & VT_LVAL) && (r < VT_CONST) && (reg_classes[r] & rc);
        r2_ok = !rc2 || ((vtop->r2 < VT_CONST) && (reg_classes[vtop->r2] & rc2));

        if (!r_ok || !r2_ok) {

            if (!r_ok) {
                if (1 /* we can 'mov (r),r' in cases */
                    && r < VT_CONST
                    && (reg_classes[r] & rc)
                    && !rc2
                    )
                    save_reg_upstack(r, 1);
                else
                    r = get_reg(rc);
            }

            if (rc2) {
                int load_type = (bt == VT_QFLOAT) ? VT_DOUBLE : VT_PTRDIFF_T;
                int original_type = vtop->type.t;

                /* two register type load :
                   expand to two words temporarily */
                if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
                    /* load constant */
                    unsigned long long ll = vtop->c.i;
                    vtop->c.i = ll; /* first word */
                    load(r, vtop);
                    vtop->r = r; /* save register value */
                    vpushi(ll >> 32); /* second word */
                } else if (vtop->r & VT_LVAL) {
                    /* We do not want to modifier the long long pointer here.
                       So we save any other instances down the stack */
                    save_reg_upstack(vtop->r, 1);
                    /* load from memory */
                    vtop->type.t = load_type;
                    load(r, vtop);
                    vdup();
                    vtop[-1].r = r; /* save register value */
                    /* increment pointer to get second word */
                    incr_offset(PTR_SIZE);
                } else {
                    /* move registers */
                    if (!r_ok)
                        load(r, vtop);
                    if (r2_ok && vtop->r2 < VT_CONST)
                        goto done;
                    vdup();
                    vtop[-1].r = r; /* save register value */
                    vtop->r = vtop[-1].r2;
                }
                /* Allocate second register. Here we rely on the fact that
                   get_reg() tries first to free r2 of an SValue. */
                r2 = get_reg(rc2);
                load(r2, vtop);
                vpop();
                /* write second register */
                vtop->r2 = r2;
            done:
                vtop->type.t = original_type;
            } else {
                if (vtop->r == VT_CMP)
                    vset_VT_JMP();
                /* one register type load */
                load(r, vtop);
            }
        }
        vtop->r = r;
#ifdef TCC_TARGET_C67
        /* uses register pairs for doubles */
        if (bt == VT_DOUBLE)
            vtop->r2 = r+1;
#endif
    }
    return r;
}

/* generate vtop[-1] and vtop[0] in resp. classes rc1 and rc2 */
ST_FUNC void gv2(int rc1, int rc2)
{
    /* generate more generic register first. But VT_JMP or VT_CMP
       values must be generated first in all cases to avoid possible
       reload errors */
    if (vtop->r != VT_CMP && rc1 <= rc2) {
        vswap();
        gv(rc1);
        vswap();
        gv(rc2);
        /* test if reload is needed for first register */
        if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
            vswap();
            gv(rc1);
            vswap();
        }
    } else {
        gv(rc2);
        vswap();
        gv(rc1);
        vswap();
        /* test if reload is needed for first register */
        if ((vtop[0].r & VT_VALMASK) >= VT_CONST) {
            gv(rc2);
        }
    }
}

#if PTR_SIZE == 4
/* expand 64bit on stack in two ints */
ST_FUNC void lexpand(void)
{
    int u, v;
    u = vtop->type.t & (VT_DEFSIGN | VT_UNSIGNED);
    v = vtop->r & (VT_VALMASK | VT_LVAL);
    if (v == VT_CONST) {
        vdup();
        vtop[0].c.i >>= 32;
    } else if (v == (VT_LVAL|VT_CONST) || v == (VT_LVAL|VT_LOCAL)) {
        vdup();
        vtop[0].c.i += 4;
    } else {
        gv(RC_INT);
        vdup();
        vtop[0].r = vtop[-1].r2;
        vtop[0].r2 = vtop[-1].r2 = VT_CONST;
    }
    vtop[0].type.t = vtop[-1].type.t = VT_INT | u;
}
#endif

#if PTR_SIZE == 4
/* build a long long from two ints */
static void lbuild(int t)
{
    gv2(RC_INT, RC_INT);
    vtop[-1].r2 = vtop[0].r;
    vtop[-1].type.t = t;
    vpop();
}
#endif

/* convert stack entry to register and duplicate its value in another
   register */
static void gv_dup(void)
{
    int t, rc, r;

    t = vtop->type.t;
#if PTR_SIZE == 4
    if ((t & VT_BTYPE) == VT_LLONG) {
        if (t & VT_BITFIELD) {
            gv(RC_INT);
            t = vtop->type.t;
        }
        lexpand();
        gv_dup();
        vswap();
        vrotb(3);
        gv_dup();
        vrotb(4);
        /* stack: H L L1 H1 */
        lbuild(t);
        vrotb(3);
        vrotb(3);
        vswap();
        lbuild(t);
        vswap();
        return;
    }
#endif
    /* duplicate value */
    rc = RC_TYPE(t);
    gv(rc);
    r = get_reg(rc);
    vdup();
    load(r, vtop);
    vtop->r = r;
}

#if PTR_SIZE == 4
/* generate CPU independent (unsigned) long long operations */
static void gen_opl(int op)
{
    int t, a, b, op1, c, i;
    int func;
    unsigned short reg_iret = REG_IRET;
    unsigned short reg_lret = REG_IRE2;
    SValue tmp;

    switch(op) {
    case '/':
    case TOK_PDIV:
        func = TOK___divdi3;
        goto gen_func;
    case TOK_UDIV:
        func = TOK___udivdi3;
        goto gen_func;
    case '%':
        func = TOK___moddi3;
        goto gen_mod_func;
    case TOK_UMOD:
        func = TOK___umoddi3;
    gen_mod_func:
#ifdef TCC_ARM_EABI
        reg_iret = TREG_R2;
        reg_lret = TREG_R3;
#endif
    gen_func:
        /* call generic long long function */
        vpush_helper_func(func);
        vrott(3);
        gfunc_call(2);
        vpushi(0);
        vtop->r = reg_iret;
        vtop->r2 = reg_lret;
        break;
    case '^':
    case '&':
    case '|':
    case '*':
    case '+':
    case '-':
        //pv("gen_opl A",0,2);
        t = vtop->type.t;
        vswap();
        lexpand();
        vrotb(3);
        lexpand();
        /* stack: L1 H1 L2 H2 */
        tmp = vtop[0];
        vtop[0] = vtop[-3];
        vtop[-3] = tmp;
        tmp = vtop[-2];
        vtop[-2] = vtop[-3];
        vtop[-3] = tmp;
        vswap();
        /* stack: H1 H2 L1 L2 */
        //pv("gen_opl B",0,4);
        if (op == '*') {
            vpushv(vtop - 1);
            vpushv(vtop - 1);
            gen_op(TOK_UMULL);
            lexpand();
            /* stack: H1 H2 L1 L2 ML MH */
            for(i=0;i<4;i++)
                vrotb(6);
            /* stack: ML MH H1 H2 L1 L2 */
            tmp = vtop[0];
            vtop[0] = vtop[-2];
            vtop[-2] = tmp;
            /* stack: ML MH H1 L2 H2 L1 */
            gen_op('*');
            vrotb(3);
            vrotb(3);
            gen_op('*');
            /* stack: ML MH M1 M2 */
            gen_op('+');
            gen_op('+');
        } else if (op == '+' || op == '-') {
            /* XXX: add non carry method too (for MIPS or alpha) */
            if (op == '+')
                op1 = TOK_ADDC1;
            else
                op1 = TOK_SUBC1;
            gen_op(op1);
            /* stack: H1 H2 (L1 op L2) */
            vrotb(3);
            vrotb(3);
            gen_op(op1 + 1); /* TOK_xxxC2 */
        } else {
            gen_op(op);
            /* stack: H1 H2 (L1 op L2) */
            vrotb(3);
            vrotb(3);
            /* stack: (L1 op L2) H1 H2 */
            gen_op(op);
            /* stack: (L1 op L2) (H1 op H2) */
        }
        /* stack: L H */
        lbuild(t);
        break;
    case TOK_SAR:
    case TOK_SHR:
    case TOK_SHL:
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            t = vtop[-1].type.t;
            vswap();
            lexpand();
            vrotb(3);
            /* stack: L H shift */
            c = (int)vtop->c.i;
            /* constant: simpler */
            /* NOTE: all comments are for SHL. the other cases are
               done by swapping words */
            vpop();
            if (op != TOK_SHL)
                vswap();
            if (c >= 32) {
                /* stack: L H */
                vpop();
                if (c > 32) {
                    vpushi(c - 32);
                    gen_op(op);
                }
                if (op != TOK_SAR) {
                    vpushi(0);
                } else {
                    gv_dup();
                    vpushi(31);
                    gen_op(TOK_SAR);
                }
                vswap();
            } else {
                vswap();
                gv_dup();
                /* stack: H L L */
                vpushi(c);
                gen_op(op);
                vswap();
                vpushi(32 - c);
                if (op == TOK_SHL)
                    gen_op(TOK_SHR);
                else
                    gen_op(TOK_SHL);
                vrotb(3);
                /* stack: L L H */
                vpushi(c);
                if (op == TOK_SHL)
                    gen_op(TOK_SHL);
                else
                    gen_op(TOK_SHR);
                gen_op('|');
            }
            if (op != TOK_SHL)
                vswap();
            lbuild(t);
        } else {
            /* XXX: should provide a faster fallback on x86 ? */
            switch(op) {
            case TOK_SAR:
                func = TOK___ashrdi3;
                goto gen_func;
            case TOK_SHR:
                func = TOK___lshrdi3;
                goto gen_func;
            case TOK_SHL:
                func = TOK___ashldi3;
                goto gen_func;
            }
        }
        break;
    default:
        /* compare operations */
        t = vtop->type.t;
        vswap();
        lexpand();
        vrotb(3);
        lexpand();
        /* stack: L1 H1 L2 H2 */
        tmp = vtop[-1];
        vtop[-1] = vtop[-2];
        vtop[-2] = tmp;
        /* stack: L1 L2 H1 H2 */
        if (!cur_switch || cur_switch->bsym) {
            /* avoid differnt registers being saved in branches.
               This is not needed when comparing switch cases */
            save_regs(4);
        }
        /* compare high */
        op1 = op;
        /* when values are equal, we need to compare low words. since
           the jump is inverted, we invert the test too. */
        if (op1 == TOK_LT)
            op1 = TOK_LE;
        else if (op1 == TOK_GT)
            op1 = TOK_GE;
        else if (op1 == TOK_ULT)
            op1 = TOK_ULE;
        else if (op1 == TOK_UGT)
            op1 = TOK_UGE;
        a = 0;
        b = 0;
        gen_op(op1);
        if (op == TOK_NE) {
            b = gvtst(0, 0);
        } else {
            a = gvtst(1, 0);
            if (op != TOK_EQ) {
                /* generate non equal test */
                vpushi(0);
                vset_VT_CMP(TOK_NE);
                b = gvtst(0, 0);
            }
        }
        /* compare low. Always unsigned */
        op1 = op;
        if (op1 == TOK_LT)
            op1 = TOK_ULT;
        else if (op1 == TOK_LE)
            op1 = TOK_ULE;
        else if (op1 == TOK_GT)
            op1 = TOK_UGT;
        else if (op1 == TOK_GE)
            op1 = TOK_UGE;
        gen_op(op1);
#if 0//def TCC_TARGET_I386
        if (op == TOK_NE) { gsym(b); break; }
        if (op == TOK_EQ) { gsym(a); break; }
#endif
        gvtst_set(1, a);
        gvtst_set(0, b);
        break;
    }
}
#endif

/* normalize values */
static uint64_t value64(uint64_t l1, int t)
{
    if ((t & VT_BTYPE) == VT_LLONG
        || (PTR_SIZE == 8 && (t & VT_BTYPE) == VT_PTR))
        return l1;
    else if (t & VT_UNSIGNED)
        return (uint32_t)l1;
    else
        return (uint32_t)l1 | -(l1 & 0x80000000);
}

static uint64_t gen_opic_sdiv(uint64_t a, uint64_t b)
{
    uint64_t x = (a >> 63 ? -a : a) / (b >> 63 ? -b : b);
    return (a ^ b) >> 63 ? -x : x;
}

static int gen_opic_lt(uint64_t a, uint64_t b)
{
    return (a ^ (uint64_t)1 << 63) < (b ^ (uint64_t)1 << 63);
}

/* handle integer constant optimizations and various machine
   independent opt */
static void gen_opic(int op)
{
    SValue *v1 = vtop - 1;
    SValue *v2 = vtop;
    int t1 = v1->type.t & VT_BTYPE;
    int t2 = v2->type.t & VT_BTYPE;
    int c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    int c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    uint64_t l1 = c1 ? value64(v1->c.i, v1->type.t) : 0;
    uint64_t l2 = c2 ? value64(v2->c.i, v2->type.t) : 0;
    int shm = (t1 == VT_LLONG) ? 63 : 31;
    int r;

    if (c1 && c2) {
        switch(op) {
        case '+': l1 += l2; break;
        case '-': l1 -= l2; break;
        case '&': l1 &= l2; break;
        case '^': l1 ^= l2; break;
        case '|': l1 |= l2; break;
        case '*': l1 *= l2; break;

        case TOK_PDIV:
        case '/':
        case '%':
        case TOK_UDIV:
        case TOK_UMOD:
            /* if division by zero, generate explicit division */
            if (l2 == 0) {
                if (CONST_WANTED && !NOEVAL_WANTED)
                    tcc_error("division by zero in constant");
                goto general_case;
            }
            switch(op) {
            default: l1 = gen_opic_sdiv(l1, l2); break;
            case '%': l1 = l1 - l2 * gen_opic_sdiv(l1, l2); break;
            case TOK_UDIV: l1 = l1 / l2; break;
            case TOK_UMOD: l1 = l1 % l2; break;
            }
            break;
        case TOK_SHL: l1 <<= (l2 & shm); break;
        case TOK_SHR: l1 >>= (l2 & shm); break;
        case TOK_SAR:
            l1 = (l1 >> 63) ? ~(~l1 >> (l2 & shm)) : l1 >> (l2 & shm);
            break;
            /* tests */
        case TOK_ULT: l1 = l1 < l2; break;
        case TOK_UGE: l1 = l1 >= l2; break;
        case TOK_EQ: l1 = l1 == l2; break;
        case TOK_NE: l1 = l1 != l2; break;
        case TOK_ULE: l1 = l1 <= l2; break;
        case TOK_UGT: l1 = l1 > l2; break;
        case TOK_LT: l1 = gen_opic_lt(l1, l2); break;
        case TOK_GE: l1 = !gen_opic_lt(l1, l2); break;
        case TOK_LE: l1 = !gen_opic_lt(l2, l1); break;
        case TOK_GT: l1 = gen_opic_lt(l2, l1); break;
            /* logical */
        case TOK_LAND: l1 = l1 && l2; break;
        case TOK_LOR: l1 = l1 || l2; break;
        default:
            goto general_case;
        }
        v1->c.i = value64(l1, v1->type.t);
        v1->r |= v2->r & VT_NONCONST;
        vtop--;
    } else {
        /* if commutative ops, put c2 as constant */
        if (c1 && (op == '+' || op == '&' || op == '^' || 
                   op == '|' || op == '*' || op == TOK_EQ || op == TOK_NE)) {
            vswap();
            c2 = c1; //c = c1, c1 = c2, c2 = c;
            l2 = l1; //l = l1, l1 = l2, l2 = l;
        }
        if (c1 && ((l1 == 0 &&
                    (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR)) ||
                   (l1 == -1 && op == TOK_SAR))) {
            /* treat (0 << x), (0 >> x) and (-1 >> x) as constant */
            vpop();
        } else if (c2 && ((l2 == 0 && (op == '&' || op == '*')) ||
                          (op == '|' &&
                            (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))) ||
                          (l2 == 1 && (op == '%' || op == TOK_UMOD)))) {
            /* treat (x & 0), (x * 0), (x | -1) and (x % 1) as constant */
            if (l2 == 1)
                vtop->c.i = 0;
            vswap();
            vtop--;
        } else if (c2 && (((op == '*' || op == '/' || op == TOK_UDIV ||
                          op == TOK_PDIV) &&
                           l2 == 1) ||
                          ((op == '+' || op == '-' || op == '|' || op == '^' ||
                            op == TOK_SHL || op == TOK_SHR || op == TOK_SAR) &&
                           l2 == 0) ||
                          (op == '&' &&
                            (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))))) {
            /* filter out NOP operations like x*1, x-0, x&-1... */
            vtop--;
        } else if (c2 && (op == '*' || op == TOK_PDIV || op == TOK_UDIV || op == TOK_UMOD)) {
            /* try to use shifts instead of muls or divs */
            if (l2 > 0 && (l2 & (l2 - 1)) == 0) {
                int n = -1;
                if (op == TOK_UMOD) {
                    vtop->c.i = l2 - 1;
                    op = '&';
                    goto general_case;
                }
                while (l2) {
                    l2 >>= 1;
                    n++;
                }
                vtop->c.i = n;
                if (op == '*')
                    op = TOK_SHL;
                else if (op == TOK_PDIV)
                    op = TOK_SAR;
                else
                    op = TOK_SHR;
            }
            goto general_case;
        } else if (c2 && (op == '+' || op == '-') &&
                   (r = vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM),
                    r == (VT_CONST | VT_SYM) || r == VT_LOCAL)) {
            /* symbol + constant case */
            if (op == '-')
                l2 = -l2;
	    l2 += vtop[-1].c.i;
	    /* The backends can't always deal with addends to symbols
	       larger than +-1<<31.  Don't construct such.  */
	    if ((int)l2 != l2)
	        goto general_case;
            vtop--;
            vtop->c.i = l2;
        } else {
        general_case:
                /* call low level op generator */
                if (t1 == VT_LLONG || t2 == VT_LLONG ||
                    (PTR_SIZE == 8 && (t1 == VT_PTR || t2 == VT_PTR)))
                    gen_opl(op);
                else
                    gen_opi(op);
        }
        if (vtop->r == VT_CONST)
            vtop->r |= VT_NONCONST; /* is const, but only by optimization */
    }
}

#if defined TCC_TARGET_X86_64 || defined TCC_TARGET_I386 || defined TCC_TARGET_ARM64
# define gen_negf gen_opf
#elif defined TCC_TARGET_ARM
void gen_negf(int op)
{
    /* arm will detect 0-x and replace by vneg */
    vpushi(0), vswap(), gen_op('-');
}
#else
/* XXX: implement in gen_opf() for other backends too */
void gen_negf(int op)
{
    /* In IEEE negate(x) isn't subtract(0,x).  Without NaNs it's
       subtract(-0, x), but with them it's really a sign flip
       operation.  We implement this with bit manipulation and have
       to do some type reinterpretation for this, which TCC can do
       only via memory.  */
    int align, size, bt;
    size = type_size(&vtop->type, &align);
    bt = vtop->type.t & VT_BTYPE;
#if defined TCC_TARGET_X86_64 || defined TCC_TARGET_I386
    /* sizeof long double is 12 or 16 here, but it's really the 80bit
       extended float format.  */
    if (bt == VT_LDOUBLE)
        size = 10;
#endif
    if (nocode_wanted) /* save_reg() wont work */
        goto gv2;
    save_reg(gv(RC_TYPE(bt)));
    vdup();
    incr_bf_adr(size - 1);
    vdup();
    vpushi(0x80); /* flip sign */
    gen_op('^');
    vstore();
    vpop();
gv2:
    gv(RC_TYPE(bt)); /* -n is not a lvalue */
}
#endif

/* generate a floating point operation with constant propagation */
static void gen_opif(int op)
{
    int c1, c2, i, bt;
    SValue *v1, *v2;
#if defined _MSC_VER && defined __x86_64__
    /* avoid bad optimization with f1 -= f2 for f1:-0.0, f2:0.0 */
    volatile
#endif
    long double f1, f2;

    v1 = vtop - 1;
    v2 = vtop;
    if (op == TOK_NEG)
        v1 = v2;
    bt = v1->type.t & VT_BTYPE;

    /* currently, we cannot do computations with forward symbols */
    c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    if (c1 && c2) {
        if (bt == VT_FLOAT) {
            f1 = v1->c.f;
            f2 = v2->c.f;
        } else if (bt == VT_DOUBLE) {
            f1 = v1->c.d;
            f2 = v2->c.d;
        } else {
            f1 = v1->c.ld;
            f2 = v2->c.ld;
        }
        /* NOTE: we only do constant propagation if finite number (not
           NaN or infinity) (ANSI spec) */
        if (!(ieee_finite(f1) || !ieee_finite(f2)) && !CONST_WANTED)
            goto general_case;
        switch(op) {
        case '+': f1 += f2; break;
        case '-': f1 -= f2; break;
        case '*': f1 *= f2; break;
        case '/': 
            if (f2 == 0.0) {
                union { float f; unsigned u; } x1, x2, y;
		/* If not in initializer we need to potentially generate
		   FP exceptions at runtime, otherwise we want to fold.  */
                if (!CONST_WANTED)
                    goto general_case;
                /* the run-time result of 0.0/0.0 on x87, also of other compilers
                   when used to compile the f1 /= f2 below, would be -nan */
                x1.f = f1, x2.f = f2;
                if (f1 == 0.0)
                    y.u = 0x7fc00000; /* nan */
                else
                    y.u = 0x7f800000; /* infinity */
                y.u |= (x1.u ^ x2.u) & 0x80000000; /* set sign */
                f1 = y.f;
                break;
            }
            f1 /= f2;
            break;
        case TOK_NEG:
            f1 = -f1;
            goto unary_result;
        case TOK_EQ:
            i = f1 == f2;
	make_int:
            vtop -= 2;
            vpushi(i);
            return;
        case TOK_NE:
            i = f1 != f2;
	    goto make_int;
        case TOK_LT:
            i = f1 < f2;
	    goto make_int;
        case TOK_GE:
            i = f1 >= f2;
	    goto make_int;
        case TOK_LE:
            i = f1 <= f2;
	    goto make_int;
        case TOK_GT:
            i = f1 > f2;
	    goto make_int;
        default:
            goto general_case;
        }
        vtop--;
    unary_result:
        /* XXX: overflow test ? */
        if (bt == VT_FLOAT) {
            v1->c.f = f1;
        } else if (bt == VT_DOUBLE) {
            v1->c.d = f1;
        } else {
            v1->c.ld = f1;
        }
    } else {
    general_case:
        if (op == TOK_NEG) {
            gen_negf(op);
        } else {
            gen_opf(op);
        }
    }
}

/* print a type. If 'varstr' is not NULL, then the variable is also
   printed in the type */
/* XXX: union */
/* XXX: add array and function pointers */
static void type_to_str(char *buf, int buf_size,
                 CType *type, const char *varstr)
{
    int bt, v, t;
    Sym *s, *sa;
    char buf1[256];
    const char *tstr;

    t = type->t;
    bt = t & VT_BTYPE;
    buf[0] = '\0';

    if (t & VT_EXTERN)
        pstrcat(buf, buf_size, "extern ");
    if (t & VT_STATIC)
        pstrcat(buf, buf_size, "static ");
    if (t & VT_TYPEDEF)
        pstrcat(buf, buf_size, "typedef ");
    if (t & VT_INLINE)
        pstrcat(buf, buf_size, "inline ");
    if (bt != VT_PTR) {
        if (t & VT_VOLATILE)
            pstrcat(buf, buf_size, "volatile ");
        if (t & VT_CONSTANT)
            pstrcat(buf, buf_size, "const ");
    }
    if (((t & VT_DEFSIGN) && bt == VT_BYTE)
        || ((t & VT_UNSIGNED)
            && (bt == VT_SHORT || bt == VT_INT || bt == VT_LLONG)
            && !IS_ENUM(t)
            ))
        pstrcat(buf, buf_size, (t & VT_UNSIGNED) ? "unsigned " : "signed ");

    buf_size -= strlen(buf);
    buf += strlen(buf);

    switch(bt) {
    case VT_VOID:
        tstr = "void";
        goto add_tstr;
    case VT_BOOL:
        tstr = "_Bool";
        goto add_tstr;
    case VT_BYTE:
        tstr = "char";
        goto add_tstr;
    case VT_SHORT:
        tstr = "short";
        goto add_tstr;
    case VT_INT:
        tstr = "int";
        goto maybe_long;
    case VT_LLONG:
        tstr = "long long";
    maybe_long:
        if (t & VT_LONG)
            tstr = "long";
        if (!IS_ENUM(t))
            goto add_tstr;
        tstr = "enum ";
        goto tstruct;
    case VT_FLOAT:
        tstr = "float";
        goto add_tstr;
    case VT_DOUBLE:
        tstr = "double";
        if (!(t & VT_LONG))
            goto add_tstr;
    case VT_LDOUBLE:
        tstr = "long double";
    add_tstr:
        pstrcat(buf, buf_size, tstr);
        break;
    case VT_STRUCT:
        tstr = "struct ";
        if (IS_UNION(t))
            tstr = "union ";
    tstruct:
        pstrcat(buf, buf_size, tstr);
        v = type->ref->v & ~SYM_STRUCT;
        if (v >= SYM_FIRST_ANOM)
            pstrcat(buf, buf_size, "<anonymous>");
        else
            pstrcat(buf, buf_size, get_tok_str(v, NULL));
        break;
    case VT_FUNC:
        s = type->ref;
        buf1[0]=0;
        if (varstr && '*' == *varstr) {
            pstrcat(buf1, sizeof(buf1), "(");
            pstrcat(buf1, sizeof(buf1), varstr);
            pstrcat(buf1, sizeof(buf1), ")");
        }
        pstrcat(buf1, buf_size, "(");
        sa = s->next;
        while (sa != NULL) {
            char buf2[256];
            type_to_str(buf2, sizeof(buf2), &sa->type, NULL);
            pstrcat(buf1, sizeof(buf1), buf2);
            sa = sa->next;
            if (sa)
                pstrcat(buf1, sizeof(buf1), ", ");
        }
        if (s->f.func_type == FUNC_ELLIPSIS)
            pstrcat(buf1, sizeof(buf1), ", ...");
        pstrcat(buf1, sizeof(buf1), ")");
        type_to_str(buf, buf_size, &s->type, buf1);
        goto no_var;
    case VT_PTR:
        s = type->ref;
        if (t & (VT_ARRAY|VT_VLA)) {
            if (varstr && '*' == *varstr)
                snprintf(buf1, sizeof(buf1), "(%s)[%d]", varstr, s->c);
            else
                snprintf(buf1, sizeof(buf1), "%s[%d]", varstr ? varstr : "", s->c);
            type_to_str(buf, buf_size, &s->type, buf1);
            goto no_var;
        }
        pstrcpy(buf1, sizeof(buf1), "*");
        if (t & VT_CONSTANT)
            pstrcat(buf1, buf_size, "const ");
        if (t & VT_VOLATILE)
            pstrcat(buf1, buf_size, "volatile ");
        if (varstr)
            pstrcat(buf1, sizeof(buf1), varstr);
        type_to_str(buf, buf_size, &s->type, buf1);
        goto no_var;
    }
    if (varstr) {
        pstrcat(buf, buf_size, " ");
        pstrcat(buf, buf_size, varstr);
    }
 no_var: ;
}

static void type_incompatibility_error(CType* st, CType* dt, const char* fmt)
{
    char buf1[256], buf2[256];
    type_to_str(buf1, sizeof(buf1), st, NULL);
    type_to_str(buf2, sizeof(buf2), dt, NULL);
    tcc_error(fmt, buf1, buf2);
}

static void type_incompatibility_warning(CType* st, CType* dt, const char* fmt)
{
    char buf1[256], buf2[256];
    type_to_str(buf1, sizeof(buf1), st, NULL);
    type_to_str(buf2, sizeof(buf2), dt, NULL);
    tcc_warning(fmt, buf1, buf2);
}

static int pointed_size(CType *type)
{
    int align;
    return type_size(pointed_type(type), &align);
}

static inline int is_null_pointer(SValue *p)
{
    if ((p->r & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST)
        return 0;
    return ((p->type.t & VT_BTYPE) == VT_INT && (uint32_t)p->c.i == 0) ||
        ((p->type.t & VT_BTYPE) == VT_LLONG && p->c.i == 0) ||
        ((p->type.t & VT_BTYPE) == VT_PTR &&
         (PTR_SIZE == 4 ? (uint32_t)p->c.i == 0 : p->c.i == 0) &&
         ((pointed_type(&p->type)->t & VT_BTYPE) == VT_VOID) &&
         0 == (pointed_type(&p->type)->t & (VT_CONSTANT | VT_VOLATILE))
         );
}

/* compare function types. OLD functions match any new functions */
static int is_compatible_func(CType *type1, CType *type2)
{
    Sym *s1, *s2;

    s1 = type1->ref;
    s2 = type2->ref;
    if (s1->f.func_call != s2->f.func_call)
        return 0;
    if (s1->f.func_type != s2->f.func_type
        && s1->f.func_type != FUNC_OLD
        && s2->f.func_type != FUNC_OLD)
        return 0;
    for (;;) {
        if (!is_compatible_unqualified_types(&s1->type, &s2->type))
            return 0;
        if (s1->f.func_type == FUNC_OLD || s2->f.func_type == FUNC_OLD )
            return 1;
        s1 = s1->next;
        s2 = s2->next;
        if (!s1)
            return !s2;
        if (!s2)
            return 0;
    }
}

/* return true if type1 and type2 are the same.  If unqualified is
   true, qualifiers on the types are ignored.
 */
static int compare_types(CType *type1, CType *type2, int unqualified)
{
    int bt1, t1, t2;

    if (IS_ENUM(type1->t)) {
        if (IS_ENUM(type2->t))
            return type1->ref == type2->ref;
        type1 = &type1->ref->type;
    } else if (IS_ENUM(type2->t))
        type2 = &type2->ref->type;

    t1 = type1->t & VT_TYPE;
    t2 = type2->t & VT_TYPE;
    if (unqualified) {
        /* strip qualifiers before comparing */
        t1 &= ~(VT_CONSTANT | VT_VOLATILE);
        t2 &= ~(VT_CONSTANT | VT_VOLATILE);
    }

    /* Default Vs explicit signedness only matters for char */
    if ((t1 & VT_BTYPE) != VT_BYTE) {
        t1 &= ~VT_DEFSIGN;
        t2 &= ~VT_DEFSIGN;
    }
    /* XXX: bitfields ? */
    if (t1 != t2)
        return 0;

    if ((t1 & VT_ARRAY)
        && !(type1->ref->c < 0
          || type2->ref->c < 0
          || type1->ref->c == type2->ref->c))
            return 0;

    /* test more complicated cases */
    bt1 = t1 & VT_BTYPE;
    if (bt1 == VT_PTR) {
        type1 = pointed_type(type1);
        type2 = pointed_type(type2);
        return is_compatible_types(type1, type2);
    } else if (bt1 == VT_STRUCT) {
        return (type1->ref == type2->ref);
    } else if (bt1 == VT_FUNC) {
        return is_compatible_func(type1, type2);
    } else {
        return 1;
    }
}

#define CMP_OP 'C'
#define SHIFT_OP 'S'

/* Check if OP1 and OP2 can be "combined" with operation OP, the combined
   type is stored in DEST if non-null (except for pointer plus/minus) . */
static int combine_types(CType *dest, SValue *op1, SValue *op2, int op)
{
    CType *type1, *type2, type;
    int t1, t2, bt1, bt2;
    int ret = 1;

    /* for shifts, 'combine' only left operand */
    if (op == SHIFT_OP)
        op2 = op1;

    type1 = &op1->type, type2 = &op2->type;
    t1 = type1->t, t2 = type2->t;
    bt1 = t1 & VT_BTYPE, bt2 = t2 & VT_BTYPE;

    type.t = VT_VOID;
    type.ref = NULL;

    if (bt1 == VT_VOID || bt2 == VT_VOID) {
        if (op != '?')
            tcc_error("operation on void value");
        /* NOTE: as an extension, we accept void on only one side */
        type.t = VT_VOID;
    } else if (bt1 == VT_PTR || bt2 == VT_PTR) {
        if (op == '+') {
          if (!is_integer_btype(bt1 == VT_PTR ? bt2 : bt1))
            ret = 0;
        }
        /* http://port70.net/~nsz/c/c99/n1256.html#6.5.15p6 */
        /* If one is a null ptr constant the result type is the other.  */
        else if (is_null_pointer (op2)) type = *type1;
        else if (is_null_pointer (op1)) type = *type2;
        else if (bt1 != bt2) {
            /* accept comparison or cond-expr between pointer and integer
               with a warning */
            if ((op == '?' || op == CMP_OP)
                && (is_integer_btype(bt1) || is_integer_btype(bt2)))
              tcc_warning("pointer/integer mismatch in %s",
                          op == '?' ? "conditional expression" : "comparison");
            else if (op != '-' || !is_integer_btype(bt2))
              ret = 0;
            type = *(bt1 == VT_PTR ? type1 : type2);
        } else {
            CType *pt1 = pointed_type(type1);
            CType *pt2 = pointed_type(type2);
            int pbt1 = pt1->t & VT_BTYPE;
            int pbt2 = pt2->t & VT_BTYPE;
            int newquals, copied = 0;
            if (pbt1 != VT_VOID && pbt2 != VT_VOID
                && !compare_types(pt1, pt2, 1/*unqualif*/)) {
                if (op != '?' && op != CMP_OP)
                  ret = 0;
                else
                  type_incompatibility_warning(type1, type2,
                    op == '?'
                     ? "pointer type mismatch in conditional expression ('%s' and '%s')"
                     : "pointer type mismatch in comparison('%s' and '%s')");
            }
            if (op == '?') {
                /* pointers to void get preferred, otherwise the
                   pointed to types minus qualifs should be compatible */
                type = *((pbt1 == VT_VOID) ? type1 : type2);
                /* combine qualifs */
                newquals = ((pt1->t | pt2->t) & (VT_CONSTANT | VT_VOLATILE));
                if ((~pointed_type(&type)->t & (VT_CONSTANT | VT_VOLATILE))
                    & newquals)
                  {
                    /* copy the pointer target symbol */
                    type.ref = sym_push(SYM_FIELD, &type.ref->type,
                                        0, type.ref->c);
                    copied = 1;
                    pointed_type(&type)->t |= newquals;
                  }
                /* pointers to incomplete arrays get converted to
                   pointers to completed ones if possible */
                if (pt1->t & VT_ARRAY
                    && pt2->t & VT_ARRAY
                    && pointed_type(&type)->ref->c < 0
                    && (pt1->ref->c > 0 || pt2->ref->c > 0))
                  {
                    if (!copied)
                      type.ref = sym_push(SYM_FIELD, &type.ref->type,
                                          0, type.ref->c);
                    pointed_type(&type)->ref =
                        sym_push(SYM_FIELD, &pointed_type(&type)->ref->type,
                                 0, pointed_type(&type)->ref->c);
                    pointed_type(&type)->ref->c =
                        0 < pt1->ref->c ? pt1->ref->c : pt2->ref->c;
                  }
            }
        }
        if (op == CMP_OP)
          type.t = VT_SIZE_T;
    } else if (bt1 == VT_STRUCT || bt2 == VT_STRUCT) {
        if (op != '?' || !compare_types(type1, type2, 1))
          ret = 0;
        type = *type1;
    } else if (is_float(bt1) || is_float(bt2)) {
        if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) {
            type.t = VT_LDOUBLE;
        } else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) {
            type.t = VT_DOUBLE;
        } else {
            type.t = VT_FLOAT;
        }
    } else if (bt1 == VT_LLONG || bt2 == VT_LLONG) {
        /* cast to biggest op */
        type.t = VT_LLONG | VT_LONG;
        if (bt1 == VT_LLONG)
          type.t &= t1;
        if (bt2 == VT_LLONG)
          type.t &= t2;
        /* convert to unsigned if it does not fit in a long long */
        if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED) ||
            (t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED))
          type.t |= VT_UNSIGNED;
    } else {
        /* integer operations */
        type.t = VT_INT | (VT_LONG & (t1 | t2));
        /* convert to unsigned if it does not fit in an integer */
        if (((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED)
                && (!(t1 & VT_BITFIELD) || BIT_SIZE(t1) == 32))
         || ((t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED)
                && (!(t2 & VT_BITFIELD) || BIT_SIZE(t2) == 32)))
          type.t |= VT_UNSIGNED;
    }
    if (dest)
      *dest = type;
    return ret;
}

/* generic gen_op: handles types problems */
ST_FUNC void gen_op(int op)
{
    int t1, t2, bt1, bt2, t;
    CType type1, combtype;
    int op_class = op;

    if (op == TOK_SHR || op == TOK_SAR || op == TOK_SHL)
        op_class = SHIFT_OP;
    else if (TOK_ISCOND(op)) /* == != > ... */
        op_class = CMP_OP;

redo:
    t1 = vtop[-1].type.t;
    t2 = vtop[0].type.t;
    bt1 = t1 & VT_BTYPE;
    bt2 = t2 & VT_BTYPE;
        
    if (bt1 == VT_FUNC || bt2 == VT_FUNC) {
	if (bt2 == VT_FUNC) {
	    mk_pointer(&vtop->type);
	    gaddrof();
	}
	if (bt1 == VT_FUNC) {
	    vswap();
	    mk_pointer(&vtop->type);
	    gaddrof();
	    vswap();
	}
	goto redo;
    } else if (!combine_types(&combtype, vtop - 1, vtop, op_class)) {
op_err:
        tcc_error("invalid operand types for binary operation");
    } else if (bt1 == VT_PTR || bt2 == VT_PTR) {
        /* at least one operand is a pointer */
        /* relational op: must be both pointers */
        int align;
        if (op_class == CMP_OP)
            goto std_op;
        /* if both pointers, then it must be the '-' op */
        if (bt1 == VT_PTR && bt2 == VT_PTR) {
            if (op != '-')
                goto op_err;
            vpush_type_size(pointed_type(&vtop[-1].type), &align);
            vtop->type.t &= ~VT_UNSIGNED;
            vrott(3);
            gen_opic(op);
            vtop->type.t = VT_PTRDIFF_T;
            vswap();
            gen_op(TOK_PDIV);
        } else {
            /* exactly one pointer : must be '+' or '-'. */
            if (op != '-' && op != '+')
                goto op_err;
            /* Put pointer as first operand */
            if (bt2 == VT_PTR) {
                vswap();
                t = t1, t1 = t2, t2 = t;
                bt2 = bt1;
            }
#if PTR_SIZE == 4
            if (bt2 == VT_LLONG)
                /* XXX: truncate here because gen_opl can't handle ptr + long long */
                gen_cast_s(VT_INT);
#endif
            type1 = vtop[-1].type;
            vpush_type_size(pointed_type(&vtop[-1].type), &align);
            vtop->type.t &= ~VT_UNSIGNED;
            gen_op('*');
#ifdef CONFIG_TCC_BCHECK
            if (tcc_state->do_bounds_check && !CONST_WANTED) {
                /* if bounded pointers, we generate a special code to
                   test bounds */
                if (op == '-') {
                    vpushi(0);
                    vswap();
                    gen_op('-');
                }
                gen_bounded_ptr_add();
            } else
#endif
            {
                gen_opic(op);
            }
            type1.t &= ~(VT_ARRAY|VT_VLA);
            /* put again type if gen_opic() swaped operands */
            vtop->type = type1;
        }
    } else {
        /* floats can only be used for a few operations */
        if (is_float(combtype.t)
            && op != '+' && op != '-' && op != '*' && op != '/'
            && op_class != CMP_OP) {
            goto op_err;
        }
    std_op:
        t = t2 = combtype.t;
        /* special case for shifts and long long: we keep the shift as
           an integer */
        if (op_class == SHIFT_OP)
            t2 = VT_INT;
        /* XXX: currently, some unsigned operations are explicit, so
           we modify them here */
        if (t & VT_UNSIGNED) {
            if (op == TOK_SAR)
                op = TOK_SHR;
            else if (op == '/')
                op = TOK_UDIV;
            else if (op == '%')
                op = TOK_UMOD;
            else if (op == TOK_LT)
                op = TOK_ULT;
            else if (op == TOK_GT)
                op = TOK_UGT;
            else if (op == TOK_LE)
                op = TOK_ULE;
            else if (op == TOK_GE)
                op = TOK_UGE;
        }
        vswap();
        gen_cast_s(t);
        vswap();
        gen_cast_s(t2);
        if (is_float(t))
            gen_opif(op);
        else
            gen_opic(op);
        if (op_class == CMP_OP) {
            /* relational op: the result is an int */
            vtop->type.t = VT_INT;
        } else {
            vtop->type.t = t;
        }
    }
    // Make sure that we have converted to an rvalue:
    if (vtop->r & VT_LVAL)
        gv(RC_TYPE(vtop->type.t));
}

#if defined TCC_TARGET_ARM64 || defined TCC_TARGET_RISCV64 || defined TCC_TARGET_ARM
#define gen_cvt_itof1 gen_cvt_itof
#else
/* generic itof for unsigned long long case */
static void gen_cvt_itof1(int t)
{
    if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) == 
        (VT_LLONG | VT_UNSIGNED)) {

        if (t == VT_FLOAT)
            vpush_helper_func(TOK___floatundisf);
#if LDOUBLE_SIZE != 8
        else if (t == VT_LDOUBLE)
            vpush_helper_func(TOK___floatundixf);
#endif
        else
            vpush_helper_func(TOK___floatundidf);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        PUT_R_RET(vtop, t);
    } else {
        gen_cvt_itof(t);
    }
}
#endif

#if defined TCC_TARGET_ARM64 || defined TCC_TARGET_RISCV64
#define gen_cvt_ftoi1 gen_cvt_ftoi
#else
/* generic ftoi for unsigned long long case */
static void gen_cvt_ftoi1(int t)
{
    int st;
    if (t == (VT_LLONG | VT_UNSIGNED)) {
        /* not handled natively */
        st = vtop->type.t & VT_BTYPE;
        if (st == VT_FLOAT)
            vpush_helper_func(TOK___fixunssfdi);
#if LDOUBLE_SIZE != 8
        else if (st == VT_LDOUBLE)
            vpush_helper_func(TOK___fixunsxfdi);
#endif
        else
            vpush_helper_func(TOK___fixunsdfdi);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        PUT_R_RET(vtop, t);
    } else {
        gen_cvt_ftoi(t);
    }
}
#endif

/* special delayed cast for char/short */
static void force_charshort_cast(void)
{
    int sbt = BFGET(vtop->r, VT_MUSTCAST) == 2 ? VT_LLONG : VT_INT;
    int dbt = vtop->type.t;
    vtop->r &= ~VT_MUSTCAST;
    vtop->type.t = sbt;
    gen_cast_s(dbt == VT_BOOL ? VT_BYTE|VT_UNSIGNED : dbt);
    vtop->type.t = dbt;
}

static void gen_cast_s(int t)
{
    CType type;
    type.t = t;
    type.ref = NULL;
    gen_cast(&type);
}

/* cast 'vtop' to 'type'. Casting to bitfields is forbidden. */
static void gen_cast(CType *type)
{
    int sbt, dbt, sf, df, c;
    int dbt_bt, sbt_bt, ds, ss, bits, trunc;

    /* special delayed cast for char/short */
    if (vtop->r & VT_MUSTCAST)
        force_charshort_cast();

    /* bitfields first get cast to ints */
    if (vtop->type.t & VT_BITFIELD)
        gv(RC_INT);

    if (IS_ENUM(type->t) && type->ref->c < 0)
        tcc_error("cast to incomplete type");

    dbt = type->t & (VT_BTYPE | VT_UNSIGNED);
    sbt = vtop->type.t & (VT_BTYPE | VT_UNSIGNED);
    if (sbt == VT_FUNC)
        sbt = VT_PTR;

again:
    if (sbt != dbt) {
        sf = is_float(sbt);
        df = is_float(dbt);
        dbt_bt = dbt & VT_BTYPE;
        sbt_bt = sbt & VT_BTYPE;
        if (dbt_bt == VT_VOID) {
            /* do not confuse backends with VT_VOID in registers */
            vpop(), vpushi(0);
            goto done;
        }
        if (sbt_bt == VT_VOID) {
error:
            cast_error(&vtop->type, type);
        }

        c = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
        if (c) {
            /* constant case: we can do it now */
            /* XXX: in ISOC, cannot do it if error in convert */
            if (sbt == VT_FLOAT)
                vtop->c.ld = vtop->c.f;
            else if (sbt == VT_DOUBLE)
                vtop->c.ld = vtop->c.d;

            if (df) {
                if (sbt_bt == VT_LLONG) {
                    if ((sbt & VT_UNSIGNED) || !(vtop->c.i >> 63))
                        vtop->c.ld = vtop->c.i;
                    else
                        vtop->c.ld = -(long double)-vtop->c.i;
                } else if(!sf) {
                    if ((sbt & VT_UNSIGNED) || !(vtop->c.i >> 31))
                        vtop->c.ld = (uint32_t)vtop->c.i;
                    else
                        vtop->c.ld = -(long double)-(uint32_t)vtop->c.i;
                }

                if (dbt == VT_FLOAT)
                    vtop->c.f = (float)vtop->c.ld;
                else if (dbt == VT_DOUBLE)
                    vtop->c.d = (double)vtop->c.ld;
            } else if (sf && dbt == VT_BOOL) {
                vtop->c.i = (vtop->c.ld != 0);
            } else {
                if(sf) {
                    if (dbt & VT_UNSIGNED)
                        vtop->c.i = (uint64_t)vtop->c.ld;
                    else
                        vtop->c.i = (int64_t)vtop->c.ld;
                }
                else if (sbt_bt == VT_LLONG || (PTR_SIZE == 8 && sbt == VT_PTR))
                    ;
                else if (sbt & VT_UNSIGNED)
                    vtop->c.i = (uint32_t)vtop->c.i;
                else
                    vtop->c.i = ((uint32_t)vtop->c.i | -(vtop->c.i & 0x80000000));

                if (dbt_bt == VT_LLONG || (PTR_SIZE == 8 && dbt == VT_PTR))
                    ;
                else if (dbt == VT_BOOL)
                    vtop->c.i = (vtop->c.i != 0);
                else {
                    uint32_t m = dbt_bt == VT_BYTE ? 0xff :
                                 dbt_bt == VT_SHORT ? 0xffff :
                                  0xffffffff;
                    vtop->c.i &= m;
                    if (!(dbt & VT_UNSIGNED))
                        vtop->c.i |= -(vtop->c.i & ((m >> 1) + 1));
                }
            }
            goto done;

        } else if (dbt == VT_BOOL
            && (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM))
                == (VT_CONST | VT_SYM)) {
            /* addresses are considered non-zero (see tcctest.c:sinit23) */
            vtop->r = VT_CONST;
            vtop->c.i = 1;
            goto done;
        }

        /* cannot generate code for global or static initializers */
        if (nocode_wanted & DATA_ONLY_WANTED) {
            if (df)
                vtop->r = get_reg(RC_FLOAT); /* don't confuse backends */
            goto done;
        }

        /* non constant case: generate code */
        if (dbt == VT_BOOL) {
            gen_test_zero(TOK_NE);
            goto done;
        }

        if (sf || df) {
            if (sf && df) {
                /* convert from fp to fp */
                gen_cvt_ftof(dbt);
            } else if (df) {
                /* convert int to fp */
                gen_cvt_itof1(dbt);
            } else {
                /* convert fp to int */
                sbt = dbt;
                if (dbt_bt != VT_LLONG && dbt_bt != VT_INT)
                    sbt = VT_INT;
                gen_cvt_ftoi1(sbt);
                goto again; /* may need char/short cast */
            }
            goto done;
        }

        ds = btype_size(dbt_bt);
        ss = btype_size(sbt_bt);
        if (ds == 0 || ss == 0)
            goto error;

        /* same size and no sign conversion needed */
        if (ds == ss && ds >= 4)
            goto done;
        if (dbt_bt == VT_PTR || sbt_bt == VT_PTR) {
            tcc_warning("cast between pointer and integer of different size");
            if (sbt_bt == VT_PTR) {
                /* put integer type to allow logical operations below */
                vtop->type.t = (PTR_SIZE == 8 ? VT_LLONG : VT_INT);
            }
        }

        /* processor allows { int a = 0, b = *(char*)&a; }
           That means that if we cast to less width, we can just
           change the type and read it still later. */
        #define ALLOW_SUBTYPE_ACCESS 1

        if (ALLOW_SUBTYPE_ACCESS && (vtop->r & VT_LVAL)) {
            /* value still in memory */
            if (ds <= ss)
                goto done;
            /* ss <= 4 here */
            if (ds <= 4 && !(dbt == (VT_SHORT | VT_UNSIGNED) && sbt == VT_BYTE)) {
                gv(RC_INT);
                goto done; /* no 64bit envolved */
            }
        }
        gv(RC_INT);

        trunc = 0;
#if PTR_SIZE == 4
        if (ds == 8) {
            /* generate high word */
            if (sbt & VT_UNSIGNED) {
                vpushi(0);
                gv(RC_INT);
            } else {
                gv_dup();
                vpushi(31);
                gen_op(TOK_SAR);
            }
            lbuild(dbt);
        } else if (ss == 8) {
            /* from long long: just take low order word */
            lexpand();
            vpop();
        }
        ss = 4;

#elif PTR_SIZE == 8
        if (ds == 8) {
            /* need to convert from 32bit to 64bit */
            if (sbt & VT_UNSIGNED) {
#if defined(TCC_TARGET_RISCV64)
                /* RISC-V keeps 32bit vals in registers sign-extended.
                   So here we need a zero-extension.  */
                trunc = 32;
#else
                goto done;
#endif
            } else {
                gen_cvt_sxtw();
                goto done;
            }
            ss = ds, ds = 4, dbt = sbt;
        } else if (ss == 8) {
            /* RISC-V keeps 32bit vals in registers sign-extended.
               So here we need a sign-extension for signed types and
               zero-extension. for unsigned types. */
#if !defined(TCC_TARGET_RISCV64)
            trunc = 32; /* zero upper 32 bits for non RISC-V targets */
#endif
        } else {
            ss = 4;
        }
#endif

        if (ds >= ss)
            goto done;
#if defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64 || defined TCC_TARGET_ARM64 || defined TCC_TARGET_RISCV64
    if (ss == 4) {
        gen_cvt_csti(dbt);
        goto done;
    }
#endif
        bits = (ss - ds) * 8;
        /* for unsigned, gen_op will convert SAR to SHR */
        vtop->type.t = (ss == 8 ? VT_LLONG : VT_INT) | (dbt & VT_UNSIGNED);
        vpushi(bits);
        gen_op(TOK_SHL);
        vpushi(bits - trunc);
        gen_op(TOK_SAR);
        vpushi(trunc);
        gen_op(TOK_SHR);
    }
done:
    vtop->type = *type;
    vtop->type.t &= ~ ( VT_CONSTANT | VT_VOLATILE | VT_ARRAY | VT_TLS );
}

/* return type size as known at compile time. Put alignment at 'a' */
ST_FUNC int type_size(CType *type, int *a)
{
    Sym *s;
    int bt;

    bt = type->t & VT_BTYPE;
    if (bt == VT_STRUCT) {
        /* struct/union */
        s = type->ref;
        *a = s->r;
        return s->c;
    } else if (bt == VT_PTR) {
        if (type->t & VT_ARRAY) {
            int ts;
            s = type->ref;
            ts = type_size(&s->type, a);
            if (s->c < 0)
                return s->c;
            return ts * s->c;
        } else {
            *a = PTR_SIZE;
            return PTR_SIZE;
        }
    } else if (IS_ENUM(type->t) && type->ref->c < 0) {
        *a = 0;
        return -1; /* incomplete enum */
    } else if (bt == VT_LDOUBLE) {
        *a = LDOUBLE_ALIGN;
        return LDOUBLE_SIZE;
    } else if (bt == VT_DOUBLE || bt == VT_LLONG) {
#if (defined TCC_TARGET_I386 && !defined TCC_TARGET_PE) \
 || (defined TCC_TARGET_ARM && !defined TCC_ARM_EABI)
        *a = 4;
#else
        *a = 8;
#endif
        return 8;
    } else if (bt == VT_INT || bt == VT_FLOAT) {
        *a = 4;
        return 4;
    } else if (bt == VT_SHORT) {
        *a = 2;
        return 2;
    } else if (bt == VT_QLONG || bt == VT_QFLOAT) {
        *a = 8;
        return 16;
    } else {
        /* char, void, function, _Bool */
        *a = 1;
        return 1;
    }
}

/* push type size as known at runtime time on top of value stack. Put
   alignment at 'a' */
static void vpush_type_size(CType *type, int *a)
{
    if (type->t & VT_VLA) {
        type_size(&type->ref->type, a);
        vset(&int_type, VT_LOCAL|VT_LVAL, type->ref->c);
    } else {
        int size = type_size(type, a);
        if (size < 0)
            tcc_error("unknown type size");
        vpushs(size);
    }
}

/* return the pointed type of t */
static inline CType *pointed_type(CType *type)
{
    return &type->ref->type;
}

/* modify type so that its it is a pointer to type. */
ST_FUNC void mk_pointer(CType *type)
{
    Sym *s;
    s = sym_push(SYM_FIELD, type, 0, -1);
    type->t = VT_PTR | (type->t & VT_STORAGE);
    type->ref = s;
}

/* return true if type1 and type2 are exactly the same (including
   qualifiers). 
*/
static int is_compatible_types(CType *type1, CType *type2)
{
    return compare_types(type1,type2,0);
}

/* return true if type1 and type2 are the same (ignoring qualifiers).
*/
static int is_compatible_unqualified_types(CType *type1, CType *type2)
{
    return compare_types(type1,type2,1);
}

static void cast_error(CType *st, CType *dt)
{
    type_incompatibility_error(st, dt, "cannot convert '%s' to '%s'");
}

/* verify type compatibility to store vtop in 'dt' type */
static void verify_assign_cast(CType *dt)
{
    CType *st, *type1, *type2;
    int dbt, sbt, qualwarn, lvl;

    st = &vtop->type; /* source type */
    dbt = dt->t & VT_BTYPE;
    sbt = st->t & VT_BTYPE;
    if (dt->t & VT_CONSTANT)
        tcc_warning("assignment of read-only location");
    switch(dbt) {
    case VT_VOID:
        if (sbt != dbt)
            tcc_error("assignment to void expression");
        break;
    case VT_PTR:
        /* special cases for pointers */
        /* '0' can also be a pointer */
        if (is_null_pointer(vtop))
            break;
        /* accept implicit pointer to integer cast with warning */
        if (is_integer_btype(sbt)) {
            tcc_warning("assignment makes pointer from integer without a cast");
            break;
        }
        type1 = pointed_type(dt);
        if (sbt == VT_PTR)
            type2 = pointed_type(st);
        else if (sbt == VT_FUNC)
            type2 = st; /* a function is implicitly a function pointer */
        else
            goto error;
        if (is_compatible_types(type1, type2))
            break;
        for (qualwarn = lvl = 0;; ++lvl) {
            if (((type2->t & VT_CONSTANT) && !(type1->t & VT_CONSTANT)) ||
                ((type2->t & VT_VOLATILE) && !(type1->t & VT_VOLATILE)))
                qualwarn = 1;
            dbt = type1->t & (VT_BTYPE|VT_LONG);
            sbt = type2->t & (VT_BTYPE|VT_LONG);
            if (dbt != VT_PTR || sbt != VT_PTR)
                break;
            type1 = pointed_type(type1);
            type2 = pointed_type(type2);
        }
        if (!is_compatible_unqualified_types(type1, type2)) {
            if ((dbt == VT_VOID || sbt == VT_VOID) && lvl == 0) {
                /* void * can match anything */
            } else if (dbt == sbt
                && is_integer_btype(sbt & VT_BTYPE)
                && IS_ENUM(type1->t) + IS_ENUM(type2->t)
                    + !!((type1->t ^ type2->t) & VT_UNSIGNED) < 2) {
		/* Like GCC don't warn by default for merely changes
		   in pointer target signedness.  Do warn for different
		   base types, though, in particular for unsigned enums
		   and signed int targets.  */
            } else {
                tcc_warning("assignment from incompatible pointer type");
                break;
            }
        }
        if (qualwarn)
            tcc_warning_c(warn_discarded_qualifiers)("assignment discards qualifiers from pointer target type");
        break;
    case VT_BYTE:
    case VT_SHORT:
    case VT_INT:
    case VT_LLONG:
        if (sbt == VT_PTR || sbt == VT_FUNC) {
            tcc_warning("assignment makes integer from pointer without a cast");
        } else if (sbt == VT_STRUCT) {
            goto case_VT_STRUCT;
        }
        /* XXX: more tests */
        break;
    case VT_STRUCT:
    case_VT_STRUCT:
        if (!is_compatible_unqualified_types(dt, st)) {
    error:
            cast_error(st, dt);
        }
        break;
    }
}

static void gen_assign_cast(CType *dt)
{
    verify_assign_cast(dt);
    gen_cast(dt);
}

/* store vtop in lvalue pushed on stack */
ST_FUNC void vstore(void)
{
    int sbt, dbt, ft, r, size, align, bit_size, bit_pos, delayed_cast;

    ft = vtop[-1].type.t;
    sbt = vtop->type.t & VT_BTYPE;
    dbt = ft & VT_BTYPE;
    verify_assign_cast(&vtop[-1].type);

    if (sbt == VT_STRUCT) {
        /* if structure, only generate pointer */
        /* structure assignment : generate memcpy */
        size = type_size(&vtop->type, &align);
        /* destination, keep on stack() as result */
        vpushv(vtop - 1);
#ifdef CONFIG_TCC_BCHECK
        if (vtop->r & VT_MUSTBOUND)
            gbound(); /* check would be wrong after gaddrof() */
#endif
        vtop->type.t = VT_PTR;
        gaddrof();
        /* source */
        vswap();
#ifdef CONFIG_TCC_BCHECK
        if (vtop->r & VT_MUSTBOUND)
            gbound();
#endif
        vtop->type.t = VT_PTR;
        gaddrof();

#ifdef TCC_TARGET_NATIVE_STRUCT_COPY
        if (1
#ifdef CONFIG_TCC_BCHECK
            && !tcc_state->do_bounds_check
#endif
            ) {
            gen_struct_copy(size);
        } else
#endif
        {
            /* type size */
            vpushi(size);
            /* Use memmove, rather than memcpy, as dest and src may be same: */
#ifdef TCC_ARM_EABI
            if(!(align & 7))
                vpush_helper_func(TOK_memmove8);
            else if(!(align & 3))
                vpush_helper_func(TOK_memmove4);
            else
#endif
            vpush_helper_func(TOK_memmove);
            vrott(4);
            gfunc_call(3);
        }

    } else if (ft & VT_BITFIELD) {
        /* bitfield store handling */

        /* save lvalue as expression result (example: s.b = s.a = n;) */
        vdup(), vtop[-1] = vtop[-2];

        bit_pos = BIT_POS(ft);
        bit_size = BIT_SIZE(ft);
        /* remove bit field info to avoid loops */
        vtop[-1].type.t = ft & ~VT_STRUCT_MASK;

        if (dbt == VT_BOOL) {
            gen_cast(&vtop[-1].type);
            vtop[-1].type.t = (vtop[-1].type.t & ~VT_BTYPE) | (VT_BYTE | VT_UNSIGNED);
        }
        r = adjust_bf(vtop - 1, bit_pos, bit_size);
        if (dbt != VT_BOOL) {
            gen_cast(&vtop[-1].type);
            dbt = vtop[-1].type.t & VT_BTYPE;
        }
        if (r == VT_STRUCT) {
            store_packed_bf(bit_pos, bit_size);
        } else {
            unsigned long long mask = (1ULL << bit_size) - 1;
            if (dbt != VT_BOOL) {
                /* mask source */
                if (dbt == VT_LLONG)
                    vpushll(mask);
                else
                    vpushi((unsigned)mask);
                gen_op('&');
            }
            /* shift source */
            vpushi(bit_pos);
            gen_op(TOK_SHL);
            vswap();
            /* duplicate destination */
            vdup();
            vrott(3);
            /* load destination, mask and or with source */
            if (dbt == VT_LLONG)
                vpushll(~(mask << bit_pos));
            else
                vpushi(~((unsigned)mask << bit_pos));
            gen_op('&');
            gen_op('|');
            /* store result */
            vstore();
            /* ... and discard */
            vpop();
        }
    } else if (dbt == VT_VOID) {
        --vtop;
    } else {
            /* optimize char/short casts */
            delayed_cast = 0;
            if ((dbt == VT_BYTE || dbt == VT_SHORT)
                && is_integer_btype(sbt)
                ) {
                if ((vtop->r & VT_MUSTCAST)
                    && btype_size(dbt) > btype_size(sbt)
                    )
                    force_charshort_cast();
                delayed_cast = 1;
            } else {
                gen_cast(&vtop[-1].type);
            }

#ifdef CONFIG_TCC_BCHECK
            /* bound check case */
            if (vtop[-1].r & VT_MUSTBOUND) {
                vswap();
                gbound();
                vswap();
            }
#endif
            gv(RC_TYPE(dbt)); /* generate value */

            if (delayed_cast) {
                vtop->r |= BFVAL(VT_MUSTCAST, (sbt == VT_LLONG) + 1);
                //tcc_warning("deley cast %x -> %x", sbt, dbt);
                vtop->type.t = ft & VT_TYPE;
            }

            /* if lvalue was saved on stack, must read it */
            if ((vtop[-1].r & VT_VALMASK) == VT_LLOCAL) {
                SValue sv;
                r = get_reg(RC_INT);
                sv.type.t = VT_PTRDIFF_T;
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.i = vtop[-1].c.i;
		sv.sym = NULL;
                load(r, &sv);
                vtop[-1].r = r | VT_LVAL;
            }

            r = vtop->r & VT_VALMASK;
            /* two word case handling :
               store second register at word + 4 (or +8 for x86-64)  */
            if (USING_TWO_WORDS(dbt)) {
                int load_type = (dbt == VT_QFLOAT) ? VT_DOUBLE : VT_PTRDIFF_T;
                vtop[-1].type.t = load_type;
                store(r, vtop - 1);
                vswap();
                incr_offset(PTR_SIZE);
                vswap();
                /* XXX: it works because r2 is spilled last ! */
                store(vtop->r2, vtop - 1);
            } else {
                /* single word */
                store(r, vtop - 1);
            }
        vswap();
        vtop--; /* NOT vpop() because on x86 it would flush the fp stack */
    }
}

/* post defines POST/PRE add. c is the token ++ or -- */
ST_FUNC void inc(int post, int c)
{
    test_lvalue();
    vdup(); /* save lvalue */
    if (post) {
        gv_dup(); /* duplicate value */
        vrotb(3);
        vrotb(3);
    }
    /* add constant */
    vpushi(c - TOK_MID); 
    gen_op('+');
    vstore(); /* store value */
    if (post)
        vpop(); /* if post op, return saved value */
}

ST_FUNC CString* parse_mult_str (const char *msg)
{
    /* read the string */
    if (tok != TOK_STR)
        expect(msg);
    cstr_reset(&initstr);
    while (tok == TOK_STR) {
        /* XXX: add \0 handling too ? */
        cstr_cat(&initstr, tokc.str.data, -1);
        next();
    }
    cstr_ccat(&initstr, '\0');
    return &initstr;
}

/* If I is >= 1 and a power of two, returns log2(i)+1.
   If I is 0 returns 0.  */
ST_FUNC int exact_log2p1(int i)
{
  int ret;
  if (!i)
    return 0;
  for (ret = 1; i >= 1 << 8; ret += 8)
    i >>= 8;
  if (i >= 1 << 4)
    ret += 4, i >>= 4;
  if (i >= 1 << 2)
    ret += 2, i >>= 2;
  if (i >= 1 << 1)
    ret++;
  return ret;
}

/* Parse __attribute__((...)) GNUC extension. */
static void parse_attribute(AttributeDef *ad)
{
    int t, n;
    char *astr;
    AttributeDef ad_tmp;
    
redo:
    if (tok != TOK_ATTRIBUTE1 && tok != TOK_ATTRIBUTE2)
        return;
    if (NULL == ad) /* skip over / ignore attributes */
        ad = &ad_tmp;

    next();
    skip('(');
    skip('(');
    while (tok != ')') {
        if (tok < TOK_IDENT)
            expect("attribute name");
        t = tok;
        next();
        switch(t) {
	case TOK_CLEANUP1:
	case TOK_CLEANUP2:
	{
	    Sym *s;

	    skip('(');
	    s = sym_find(tok);
	    if (!s) {
	        tcc_warning_c(warn_implicit_function_declaration)(
                    "implicit declaration of function '%s'", get_tok_str(tok, &tokc));
	        s = external_global_sym(tok, &func_old_type);
            } else if ((s->type.t & VT_BTYPE) != VT_FUNC)
                tcc_error("'%s' is not declared as function", get_tok_str(tok, &tokc));
	    ad->cleanup_func = s;
	    next();
            skip(')');
	    break;
	}
        case TOK_CONSTRUCTOR1:
        case TOK_CONSTRUCTOR2:
            ad->f.func_ctor = 1;
            break;
        case TOK_DESTRUCTOR1:
        case TOK_DESTRUCTOR2:
            ad->f.func_dtor = 1;
            break;
        case TOK_ALWAYS_INLINE1:
        case TOK_ALWAYS_INLINE2:
            ad->f.func_alwinl = 1;
            break;
        case TOK_SECTION1:
        case TOK_SECTION2:
            skip('(');
	    astr = parse_mult_str("section name")->data;
            ad->section = find_section(tcc_state, astr);
            skip(')');
            break;
        case TOK_ALIAS1:
        case TOK_ALIAS2:
            skip('(');
	    astr = parse_mult_str("alias(\"target\")")->data;
            /* save string as token, for later */
            ad->alias_target = tok_alloc_const(astr);
            skip(')');
            break;
	case TOK_VISIBILITY1:
	case TOK_VISIBILITY2:
            skip('(');
	    astr = parse_mult_str("visibility(\"default|hidden|internal|protected\")")->data;
	    if (!strcmp (astr, "default"))
	        ad->a.visibility = STV_DEFAULT;
	    else if (!strcmp (astr, "hidden"))
	        ad->a.visibility = STV_HIDDEN;
	    else if (!strcmp (astr, "internal"))
	        ad->a.visibility = STV_INTERNAL;
	    else if (!strcmp (astr, "protected"))
	        ad->a.visibility = STV_PROTECTED;
	    else
                expect("visibility(\"default|hidden|internal|protected\")");
            skip(')');
            break;
        case TOK_ALIGNED1:
        case TOK_ALIGNED2:
            if (tok == '(') {
                next();
                n = expr_const();
                if (n <= 0 || (n & (n - 1)) != 0) 
                    tcc_error("alignment must be a positive power of two");
                skip(')');
            } else {
                n = MAX_ALIGN;
            }
            ad->a.aligned = exact_log2p1(n);
	    if (n != 1 << (ad->a.aligned - 1))
	      tcc_error("alignment of %d is larger than implemented", n);
            break;
        case TOK_PACKED1:
        case TOK_PACKED2:
            ad->a.packed = 1;
            break;
        case TOK_WEAK1:
        case TOK_WEAK2:
            ad->a.weak = 1;
            break;
        case TOK_NODEBUG1:
        case TOK_NODEBUG2:
            ad->a.nodebug = 1;
            break;
        case TOK_USED1:
        case TOK_USED2:
        case TOK_UNUSED1:
        case TOK_UNUSED2:
            /* currently, no need to handle it because tcc does not
               track used/unused objects */
            break;
        case TOK_CONST1:
        case TOK_CONST2:
        case TOK_CONST3:
        case TOK_PURE1:
        case TOK_PURE2:
	    /* ignored */
            break;
        case TOK_NOINLINE:
	    /* ignored */
	    break;
        case TOK_FORMAT1:
        case TOK_FORMAT2:
	    /* ignored */
            goto skip_param;
        case TOK_NORETURN1:
        case TOK_NORETURN2:
            ad->f.func_noreturn = 1;
            break;
        case TOK_CDECL1:
        case TOK_CDECL2:
        case TOK_CDECL3:
            ad->f.func_call = FUNC_CDECL;
            break;
        case TOK_STDCALL1:
        case TOK_STDCALL2:
        case TOK_STDCALL3:
            ad->f.func_call = FUNC_STDCALL;
            break;
#ifdef TCC_TARGET_I386
        case TOK_REGPARM1:
        case TOK_REGPARM2:
            skip('(');
            n = expr_const();
            if (n > 3) 
                n = 3;
            else if (n < 0)
                n = 0;
            if (n > 0)
                ad->f.func_call = FUNC_FASTCALL1 + n - 1;
            skip(')');
            break;
        case TOK_FASTCALL1:
        case TOK_FASTCALL2:
        case TOK_FASTCALL3:
            ad->f.func_call = FUNC_FASTCALLW;
            break;
        case TOK_THISCALL1:
        case TOK_THISCALL2:
        case TOK_THISCALL3:
            ad->f.func_call = FUNC_THISCALL;
            break;
#endif
        case TOK_MODE:
            skip('(');
            switch(tok) {
                case TOK_MODE_DI:
                    ad->attr_mode = VT_LLONG + 1;
                    break;
                case TOK_MODE_QI:
                    ad->attr_mode = VT_BYTE + 1;
                    break;
                case TOK_MODE_HI:
                    ad->attr_mode = VT_SHORT + 1;
                    break;
                case TOK_MODE_SI:
                case TOK_MODE_word:
                    ad->attr_mode = VT_INT + 1;
                    break;
                default:
                    tcc_warning("__mode__(%s) not supported\n", get_tok_str(tok, NULL));
                    break;
            }
            next();
            skip(')');
            break;
        case TOK_DLLEXPORT:
            ad->a.dllexport = 1;
            break;
        case TOK_NODECORATE:
            ad->a.nodecorate = 1;
            break;
        case TOK_DLLIMPORT:
            ad->a.dllimport = 1;
            break;
        default:
            tcc_warning_c(warn_unsupported)("'%s' attribute ignored", get_tok_str(t, NULL));
            /* skip parameters */
skip_param:
            if (tok == '(') {
                int parenthesis = 0;
                do {
                    if (tok == '(') 
                        parenthesis++;
                    else if (tok == ')') 
                        parenthesis--;
                    next();
                } while (parenthesis && tok != -1);
            }
            break;
        }
        if (tok != ',')
            break;
        next();
    }
    skip(')');
    skip(')');
    goto redo;
}

static Sym * find_field (CType *type, int v, int *cumofs)
{
    Sym *s = type->ref;
    int v1 = v | SYM_FIELD;
    if (!(v & SYM_FIELD)) { /* top-level call */
        if ((type->t & VT_BTYPE) != VT_STRUCT)
            expect("struct or union");
        if (v < TOK_UIDENT)
            expect("field name");
        if (s->c < 0)
            tcc_error("dereferencing incomplete type '%s'",
                get_tok_str(s->v & ~SYM_STRUCT, 0));
    }
    while ((s = s->next) != NULL) {
        if (s->v == v1) {
            *cumofs = s->c;
            return s;
        }
        if ((s->type.t & VT_BTYPE) == VT_STRUCT
          && s->v >= (SYM_FIRST_ANOM | SYM_FIELD)) {
            /* try to find field in anonymous sub-struct/union */
            Sym *ret = find_field (&s->type, v1, cumofs);
            if (ret) {
                *cumofs += s->c;
                return ret;
            }
        }
    }
    if (!(v & SYM_FIELD))
        tcc_error("field not found: %s", get_tok_str(v, NULL));
    return s;
}

static void check_fields (CType *type, int check)
{
    Sym *s = type->ref;

    while ((s = s->next) != NULL) {
        int v = s->v & ~SYM_FIELD;
        if (v < SYM_FIRST_ANOM) {
            TokenSym *ts = table_ident[v - TOK_IDENT];
            if (check && (ts->tok & SYM_FIELD))
                tcc_error("duplicate member '%s'", get_tok_str(v, NULL));
            ts->tok ^= SYM_FIELD;
        } else if ((s->type.t & VT_BTYPE) == VT_STRUCT)
            check_fields (&s->type, check);
    }
}

static void struct_layout(CType *type, AttributeDef *ad)
{
    int size, align, maxalign, offset, c, bit_pos, bit_size;
    int packed, a, bt, prevbt, prev_bit_size;
    int pcc = !tcc_state->ms_bitfields;
    int pragma_pack = *tcc_state->pack_stack_ptr;
    Sym *f;

    maxalign = 1;
    offset = 0;
    c = 0;
    bit_pos = 0;
    prevbt = VT_STRUCT; /* make it never match */
    prev_bit_size = 0;

//#define BF_DEBUG

    for (f = type->ref->next; f; f = f->next) {
        if (f->type.t & VT_BITFIELD)
            bit_size = BIT_SIZE(f->type.t);
        else
            bit_size = -1;
        size = type_size(&f->type, &align);
        a = f->a.aligned ? 1 << (f->a.aligned - 1) : 0;
        packed = 0;

        if (pcc && bit_size == 0) {
            /* in pcc mode, packing does not affect zero-width bitfields */

        } else {
            /* in pcc mode, attribute packed overrides if set. */
            if (pcc && (f->a.packed || ad->a.packed))
                align = packed = 1;

            /* pragma pack overrides align if lesser and packs bitfields always */
            if (pragma_pack) {
                packed = 1;
                if (pragma_pack < align)
                    align = pragma_pack;
                /* in pcc mode pragma pack also overrides individual align */
                if (pcc && pragma_pack < a)
                    a = 0;
            }
        }
        /* some individual align was specified */
        if (a)
            align = a;

        if (type->ref->type.t == VT_UNION) {
	    if (pcc && bit_size >= 0)
	        size = (bit_size + 7) >> 3;
	    offset = 0;
	    if (size > c)
	        c = size;

	} else if (bit_size < 0) {
            if (pcc)
                c += (bit_pos + 7) >> 3;
	    c = (c + align - 1) & -align;
	    offset = c;
	    if (size > 0)
	        c += size;
	    bit_pos = 0;
	    prevbt = VT_STRUCT;
	    prev_bit_size = 0;

	} else {
	    /* A bit-field.  Layout is more complicated.  There are two
	       options: PCC (GCC) compatible and MS compatible */
            if (pcc) {
		/* In PCC layout a bit-field is placed adjacent to the
                   preceding bit-fields, except if:
                   - it has zero-width
                   - an individual alignment was given
                   - it would overflow its base type container and
                     there is no packing */
                if (bit_size == 0) {
            new_field:
		    c = (c + ((bit_pos + 7) >> 3) + align - 1) & -align;
		    bit_pos = 0;
                } else if (f->a.aligned) {
                    goto new_field;
                } else if (!packed) {
                    int a8 = align * 8;
	            int ofs = ((c * 8 + bit_pos) % a8 + bit_size + a8 - 1) / a8;
                    if (ofs > size / align)
                        goto new_field;
                }

                /* in pcc mode, long long bitfields have type int if they fit */
                if (size == 8 && bit_size <= 32)
                    f->type.t = (f->type.t & ~VT_BTYPE) | VT_INT, size = 4;

                while (bit_pos >= align * 8)
                    c += align, bit_pos -= align * 8;
                offset = c;

		/* In PCC layout named bit-fields influence the alignment
		   of the containing struct using the base types alignment,
		   except for packed fields (which here have correct align).  */
		if (f->v & SYM_FIRST_ANOM
                    // && bit_size // ??? gcc on ARM/rpi does that
                    )
		    align = 1;

	    } else {
		bt = f->type.t & VT_BTYPE;
		if ((bit_pos + bit_size > size * 8)
                    || (bit_size > 0) == (bt != prevbt)
                    ) {
		    c = (c + align - 1) & -align;
		    offset = c;
		    bit_pos = 0;
		    /* In MS bitfield mode a bit-field run always uses
		       at least as many bits as the underlying type.
		       To start a new run it's also required that this
		       or the last bit-field had non-zero width.  */
		    if (bit_size || prev_bit_size)
		        c += size;
		}
		/* In MS layout the records alignment is normally
		   influenced by the field, except for a zero-width
		   field at the start of a run (but by further zero-width
		   fields it is again).  */
		if (bit_size == 0 && prevbt != bt)
		    align = 1;
		prevbt = bt;
                prev_bit_size = bit_size;
	    }

	    f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT))
		        | (bit_pos << VT_STRUCT_SHIFT);
	    bit_pos += bit_size;
	}
	if (align > maxalign)
	    maxalign = align;

#ifdef BF_DEBUG
	printf("set field %s offset %-2d size %-2d align %-2d",
	       get_tok_str(f->v & ~SYM_FIELD, NULL), offset, size, align);
	if (f->type.t & VT_BITFIELD) {
	    printf(" pos %-2d bits %-2d",
                    BIT_POS(f->type.t),
                    BIT_SIZE(f->type.t)
                    );
	}
	printf("\n");
#endif

        f->c = offset;
	f->r = 0;
    }

    if (pcc)
        c += (bit_pos + 7) >> 3;

    /* store size and alignment */
    a = bt = ad->a.aligned ? 1 << (ad->a.aligned - 1) : 1;
    if (a < maxalign)
        a = maxalign;
    type->ref->r = a;
    if (pragma_pack && pragma_pack < maxalign && 0 == pcc) {
        /* can happen if individual align for some member was given.  In
           this case MSVC ignores maxalign when aligning the size */
        a = pragma_pack;
        if (a < bt)
            a = bt;
    }
    c = (c + a - 1) & -a;
    type->ref->c = c;

#ifdef BF_DEBUG
    printf("struct size %-2d align %-2d\n\n", c, a), fflush(stdout);
#endif

    /* check whether we can access bitfields by their type */
    for (f = type->ref->next; f; f = f->next) {
        int s, px, cx, c0;
        CType t;

        if (0 == (f->type.t & VT_BITFIELD))
            continue;
        f->type.ref = f;
        f->auxtype = -1;
        bit_size = BIT_SIZE(f->type.t);
        if (bit_size == 0)
            continue;
        bit_pos = BIT_POS(f->type.t);
        size = type_size(&f->type, &align);

        if (bit_pos + bit_size <= size * 8 && f->c + size <= c
#ifdef TCC_TARGET_ARM
            && !(f->c & (align - 1))
#endif
            )
            continue;

        /* try to access the field using a different type */
        c0 = -1, s = align = 1;
        t.t = VT_BYTE;
        for (;;) {
            px = f->c * 8 + bit_pos;
            cx = (px >> 3) & -align;
            px = px - (cx << 3);
            if (c0 == cx)
                break;
            s = (px + bit_size + 7) >> 3;
            if (s > 4) {
                t.t = VT_LLONG;
            } else if (s > 2) {
                t.t = VT_INT;
            } else if (s > 1) {
                t.t = VT_SHORT;
            } else {
                t.t = VT_BYTE;
            }
            s = type_size(&t, &align);
            c0 = cx;
        }

        if (px + bit_size <= s * 8 && cx + s <= c
#ifdef TCC_TARGET_ARM
            && !(cx & (align - 1))
#endif
            ) {
            /* update offset and bit position */
            f->c = cx;
            bit_pos = px;
	    f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT))
		        | (bit_pos << VT_STRUCT_SHIFT);
            if (s != size)
                f->auxtype = t.t;
#ifdef BF_DEBUG
            printf("FIX field %s offset %-2d size %-2d align %-2d "
                "pos %-2d bits %-2d\n",
                get_tok_str(f->v & ~SYM_FIELD, NULL),
                cx, s, align, px, bit_size);
#endif
        } else {
            /* fall back to load/store single-byte wise */
            f->auxtype = VT_STRUCT;
#ifdef BF_DEBUG
            printf("FIX field %s : load byte-wise\n",
                 get_tok_str(f->v & ~SYM_FIELD, NULL));
#endif
        }
    }
}

/* Does 'n' fit into integer type 't' ? */
static int in_range(long long n, int t)
{
    unsigned long long m = (1ULL << (btype_size(t & VT_BTYPE) * 8 - 1)) - 1;
    if (t & VT_UNSIGNED)
        return n <= (m << 1) + 1;
    return n >= -(long long)m - 1 && n <= (long long)m;
}

/* ------------------------------------------------------------------------- */
/* 对象方法 (C++ 式: struct 体内函数定义 = 方法, 隐式 self)                  */

/* 方法定义记录: 收集期保存签名 CType + 方法体 token 流, 编译期重放 */
typedef struct MethodDef {
    struct MethodDef *next;
    int name;              /* 方法名 tok */
    CType sig;             /* 函数类型 (返回 + 参数链 ref->next, 不含 self) */
    TokenString *body;     /* 方法体 token 流 (tok_str_alloc, 字段已替换) */
    Sym *sym;              /* 内部函数符号 */
    int func_tok;          /* 内部函数名 token (__method_<id>_<名>) */
} MethodDef;

typedef struct MethodType {
    struct MethodType *next;
    Sym *ref;              /* struct 符号 */
    int id;                /* -1 = 未分配 (收集期), emit 时分配 */
    MethodDef *methods;    /* 方法链 */
    int pending;           /* 已挂待编译链 (去重) */
    struct MethodType *next_pending;  /* 待编译链 */
} MethodType;

static MethodType *method_types;   /* 方法表: 调用点查 ref → id */
static int method_id_counter;
static int method_pending_self;    /* '(' 分支的 self 注入标记 */
static int tok_self;               /* "self" 标识符 token (惰性分配) */
/* 待编译方法链表: struct_decl 收集期只注册符号, 方法体编译延迟到
   文件末尾 (gen_inline_functions 同款机制), 避免在函数中途
   gen_function 把代码插入正在生成的函数内部 */
static MethodType *method_pending;

static int method_self_tok(void)
{
    if (!tok_self)
        tok_self = tok_alloc_const("self");
    return tok_self;
}

/* 查/建类型的方法表项 (收集期 id = -1, emit 时分配) */
static MethodType *method_type_get(Sym *ref)
{
    MethodType *mt;
    for (mt = method_types; mt; mt = mt->next)
        if (mt->ref == ref)
            return mt;
    mt = tcc_malloc(sizeof *mt);
    memset(mt, 0, sizeof *mt);
    mt->ref = ref;
    mt->id = -1;
    mt->next = method_types;
    method_types = mt;
    return mt;
}

/* 调用点: 查类型的方法表 id, 无方法 → -1 */
static int method_type_id(CType *type)
{
    MethodType *mt;
    if ((type->t & VT_BTYPE) != VT_STRUCT)
        return -1;
    for (mt = method_types; mt; mt = mt->next)
        if (mt->ref == type->ref)
            return mt->id;
    return -1;
}

/* tok 是否为 struct s 已声明字段 (方法前的字段, 收集时替换用) */
static int method_is_field(Sym *s, int tok)
{
    Sym *f;
    for (f = s->next; f; f = f->next)
        if ((f->v & ~SYM_FIELD) == tok)
            return 1;
    return 0;
}

/* 收集方法定义 (tok 在方法体 '{' 上): 字段名 → self->字段 替换 + token 流保存 */
static void method_parse(Sym *s, int name, CType *type1)
{
    MethodDef *m = tcc_malloc(sizeof *m);
    MethodType *mt = method_type_get(s);
    int depth = 0;
    int prev_tok = 0;   /* 前一个 token: 遇 . / -> 时字段名不替换 (用户显式 self->x) */

    memset(m, 0, sizeof *m);
    m->name = name;
    m->sig = *type1;
    m->body = tok_str_alloc();

    for (;;) {
        if (tok == '{') {
            depth++;
            tok_str_add_tok(m->body);
            prev_tok = tok;
            next();
            continue;
        }
        if (tok == '}') {
            tok_str_add_tok(m->body);
            next();
            if (--depth == 0)
                break;
            prev_tok = tok;
            continue;
        }
        if (tok >= TOK_IDENT && method_is_field(s, tok)
            && prev_tok != '.' && prev_tok != TOK_ARROW) {
            /* 裸字段名 → self->字段; 显式 self->x / p->x 不替换 */
            int ftok = tok;
            tok = method_self_tok();
            tokc.i = tok;
            tok_str_add_tok(m->body);
            tok = TOK_ARROW;
            tokc.i = 0;
            tok_str_add_tok(m->body);
            tok = ftok;
            tokc.i = ftok;
        }
        tok_str_add_tok(m->body);
        prev_tok = tok;
        next();
    }
    /* 缓冲 token: block() 会消费 '}', gen_function 末尾 next() 再读一个。
       若流已尽, next() 会读到外部流破坏解析; 追加 ';' 让 next() 停在流内,
       由循环里的 end_macro() 显式弹出恢复。注意: 不能改全局 tok,
       直接 tok_str_add2 写入 */
    {
        CValue cv;
        cv.i = 0;
        tok_str_add2(m->body, ';', &cv);
    }
    m->next = mt->methods;
    mt->methods = m;
}

/* struct 完成后: 分配 id, 注册内部函数符号 (阶段1), 编译方法体 (阶段2) */
static void method_emit_all(Sym *s)
{
    MethodType *mt = method_type_get(s);
    MethodDef *m, *m2;
    AttributeDef ad;
    int id;

    if (!mt->methods)
        return;
    /* 方法名重复检测 */
    for (m = mt->methods; m; m = m->next)
        for (m2 = m->next; m2; m2 = m2->next)
            if (m2->name == m->name)
                tcc_error("duplicate method '%s'",
                          get_tok_str(m->name, NULL));
    id = mt->id;
    if (id < 0)
        mt->id = id = method_id_counter++;
    /* 阶段 1: 符号注册 (self 参数注入 + 内部函数名) */
    for (m = mt->methods; m; m = m->next) {
        char buf[128];
        Sym *self;
        CType st;

        snprintf(buf, sizeof buf, "__method_%d_%s", id,
                 get_tok_str(m->name, NULL));
        m->func_tok = tok_alloc_const(buf);
        /* self 参数: 类型 = struct 指针, 插到参数链头 */
        st.t = s->type.t;
        st.ref = s;
        mk_pointer(&st);
        self = tcc_malloc(sizeof *self);
        memset(self, 0, sizeof *self);
        self->v = method_self_tok();
        self->type = st;
        /* 与 decl() 创建的参数符号一致: 栈参数是 lvalue。
           gfunc_set_param 对寄存器参数 (byref=0) 只设 c 不设 r */
        self->r = VT_LOCAL | VT_LVAL;
        self->next = m->sig.ref->next;
        self->prev = m->sig.ref;
        m->sig.ref->next = self;
        if (self->next)
            self->next->prev = self;
        /* 内部函数符号 (static, 编译器合成名) */
        m->sig.t |= VT_STATIC;
        memset(&ad, 0, sizeof ad);
        m->sym = external_sym(m->func_tok, &m->sig, 0, &ad);
    }
    /* 阶段 2 延迟: 挂到待编译链, 文件末尾统一编译
       (直接在此编译会破坏正在生成的函数代码段) */
    if (!mt->pending) {
        mt->pending = 1;
        mt->next_pending = method_pending;
        method_pending = mt;
    }
}

/* 文件末尾: 编译所有待编译方法体 (gen_inline_functions 同款,
   cur_text_section 切到 text_section, gen_function 自包含) */
static void method_compile_all(void)
{
    MethodType *mt;
    MethodDef *m;

    for (mt = method_pending; mt; mt = mt->next_pending) {
        for (m = mt->methods; m; m = m->next) {
            int save_tok = tok;
            CValue save_tokc = tokc;
            begin_macro(m->body, 1);
            next();
            /* gen_inline_functions 同款: gen_function 用 cur_text_section */
            cur_text_section = text_section;
            /* gen_function 末尾 sym_pop(&local_stack, NULL, 0) 会清空
               整个局部符号栈: 文件末尾无外层局部变量, 无需隔离 */
            gen_function(m->sym);
            end_macro();
            /* gen_function 末尾 next() 会推进 tok, 恢复外部解析状态 */
            tok = save_tok;
            tokc = save_tokc;
        }
    }
    method_pending = NULL;
}

/* 调用点: v.func(…) 方法回退。tok 已在 '(' 上; 成功 → 注入 self+func, 返回 1 */
static int method_lookup(int ident)
{
    char buf[128];
    Sym *m;
    int id = method_type_id(&vtop->type);

    if (id < 0)
        return 0;
    snprintf(buf, sizeof buf, "__method_%d_%s", id,
             get_tok_str(ident, NULL));
    m = sym_find(tok_alloc(buf, strlen(buf))->tok);
    if (!m || (m->type.t & VT_BTYPE) != VT_FUNC) {
        int tv = vtop->type.ref->v & ~SYM_STRUCT;
        tcc_error("'%s' has no method '%s'",
                  tv >= SYM_FIRST_ANOM ? "<anonymous struct>"
                                       : get_tok_str(tv, NULL),
                  get_tok_str(ident, NULL));
    }
    /* self 指针入参 + 函数值压栈 → [func, self] (vtop = self) */
    gaddrof();
    mk_pointer(&vtop->type);
    vset(&m->type, m->r, m->c);
    vtop->sym = m;
    if (m->r & VT_SYM)
        vtop->c.i = 0;
    vswap();
    method_pending_self = 1;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* model 泛型 (struct/union 模板, 类型参数替换 + token 重放)                 */

/* 模型定义记录: 模板名 + 类型参数名数组 + 成员声明 token 流 */
/* 模型参数种类 */
#define MODEL_PARAM_TYPE  0   /* 裸标识符: 类型参数 (T)   */
#define MODEL_PARAM_CONST 1   /* int R 声明: 编译期整型常量参数 */

typedef struct ModelDef {
    int kind;              /* VT_STRUCT / VT_UNION / VT_FUNC */
    int name;              /* 模板名 tok */
    int nparams;           /* 参数个数 (类型 + 常量) */
    int *params;           /* 参数名 tok 数组 */
    char *param_kind;      /* MODEL_PARAM_TYPE / MODEL_PARAM_CONST */
    int  *const_tok;       /* 常量参数要发射的常量 token (TOK_CINT/TOK_CLLONG/
                              TOK_CUINT/TOK_CULLONG), 类型参数 = 0 */
    TokenString *body;     /* 成员声明/函数体 token 流 (tok_str_alloc) */
    struct ModelDef *next; /* 全局模型表 */
} ModelDef;

static ModelDef *model_list;   /* 全局模型表 */

/* model 定义已消费标志: parse_btype 返回 0 后 decl() 凭此继续 */
static int model_decl_seen;

static int model_function_call(int name, ModelDef *md);   /* unary 调用点 */

static ModelDef *model_find(int name)
{
    ModelDef *md;
    for (md = model_list; md; md = md->next)
        if (md->name == name)
            return md;
    return NULL;
}

/* 记录模式: 收集 model 定义 token 流 (tok 在 '{' 上, 含 '{' 与匹配 '}'),
   花括号深度计数以支持嵌套 struct 成员与方法体 */
static void model_collect_body(TokenString *body)
{
    int depth = 0;
    for (;;) {
        if (tok == '{') {
            depth++;
        } else if (tok == '}') {
            depth--;
            if (depth == 0) {
                tok_str_add_tok(body);
                tok_str_add(body, 0);   /* 0 结尾: 遍历靠 0 停止 */
                next();
                return;
            }
        } else if (tok == TOK_EOF) {
            expect("'}'");
        }
        tok_str_add_tok(body);
        next();
    }
}

/* 记录模式: 收集 model function 定义 token 流 (tok 在函数返回类型上),
   记录返回类型→函数名→参数列表→函数体, 到函数体匹配 '}' 结束 (含) */
static void model_collect_funcdef(TokenString *body)
{
    int depth = 0;
    for (;;) {
        if (tok == '{') {
            depth++;
        } else if (tok == '}') {
            depth--;
            if (depth == 0) {
                tok_str_add_tok(body);
                tok_str_add(body, 0);   /* 0 结尾: 遍历靠 0 停止 */
                next();
                return;
            }
        } else if (tok == TOK_EOF) {
            expect("'}'");
        }
        tok_str_add_tok(body);
        next();
    }
}

/* 是否为内置整型类型关键字 (常量参数声明 "int R" 的识别依据) */
static int is_int_type_tok(int t)
{
    switch (t) {
    case TOK_BOOL:
    case TOK_CHAR:
    case TOK_SHORT:
    case TOK_INT:
    case TOK_LONG:
    case TOK_SIGNED1:
    case TOK_SIGNED2:
    case TOK_SIGNED3:
    case TOK_UNSIGNED:
        return 1;
    }
    return 0;
}

/* 连续消费整型类型关键字序列 (int / unsigned long long / long 等), 归约为
   (大小字节, 是否无符号). tok 位于关键字序列起点. */
static void parse_const_param_type(int *size_out, int *us_out)
{
    int size = 4, us = 0;
    while (is_int_type_tok(tok)) {
        switch (tok) {
        case TOK_UNSIGNED:  us = 1; break;
        case TOK_BOOL:      size = 1; us = 1; break;   /* _Bool: 0/1 */
        case TOK_CHAR:      size = 1; break;
        case TOK_SHORT:     size = 2; break;
        case TOK_INT:       size = 4; break;
        case TOK_LONG:      size = 8; break;           /* LP64 x86_64 */
        case TOK_SIGNED1:
        case TOK_SIGNED2:
        case TOK_SIGNED3:   us = 0; break;
        }
        next();
    }
    *size_out = size;
    *us_out = us;
}

/* 常量参数替换进 body 的常量 token 码 (带符号后缀, 决定 body 中算术的整型语义) */
static int const_tok_for(int size, int us)
{
    if (size == 8)
        return us ? TOK_CULLONG : TOK_CLLONG;
    return us ? TOK_CUINT : TOK_CINT;
}

/* 记录模式: 解析 model 定义 (tok 在 TOK_STRUCT/TOK_UNION 上,
   function 泛型则是 '('), 登记到 model_list, 不生成任何代码 */
static void model_decl(void)
{
    ModelDef *md = tcc_malloc(sizeof *md);

    memset(md, 0, sizeof *md);
    if (tok == '(') {
        /* model (T) 返回类型 Name(参数) {...} — 函数泛型 */
        md->kind = VT_FUNC;
        /* 不消费: '(' 就是类型参数列表开头 */
    } else {
        md->kind = (tok == TOK_STRUCT) ? VT_STRUCT : VT_UNION;
        next();   /* struct/union */
        if (tok < TOK_IDENT)
            expect("model name");
        md->name = tok;
        next();
    }
    skip('(');
    /* 参数列表: (T1, int R, T2, ...) — 裸标识符 = 类型参数,
       整型声明 = 编译期整型常量参数 */
    while (tok != ')') {
        int kind = MODEL_PARAM_TYPE, ctok = 0, n = md->nparams;
        if (is_int_type_tok(tok)) {
            int size, us;
            parse_const_param_type(&size, &us);
            ctok = const_tok_for(size, us);
            kind = MODEL_PARAM_CONST;
        }
        if (tok < TOK_IDENT)
            expect("type parameter");
        md->params     = tcc_realloc(md->params, (n + 1) * sizeof(int));
        md->param_kind = tcc_realloc(md->param_kind, (n + 1));
        md->const_tok  = tcc_realloc(md->const_tok, (n + 1) * sizeof(int));
        md->params[n]     = tok;
        md->param_kind[n] = kind;
        md->const_tok[n]  = ctok;
        md->nparams++;
        next();
        if (tok == ',')
            next();
        else
            break;
    }
    skip(')');
    if (md->kind == VT_FUNC) {
        /* model function: tok = 返回类型。记录整个函数定义
           (返回类型→函数名→参数→体), 函数名是返回类型后
           第一个后跟 '(' 的标识符 */
        md->body = tok_str_alloc();
        model_collect_funcdef(md->body);
        /* 提取函数名: 标识符后跟 '(' 且匹配 ')' 后是 '{' 者
           (返回类型 Pair(T) 的 ')' 后是函数名, 不满足) */
        {
            const int *p = md->body->str;
            int t, prev = 0;
            CValue cv;
            while (*p) {
                const int *q = p;
                TOK_GET(&t, &p, &cv);
                if (prev >= TOK_IDENT && t == '(') {
                    /* 深度计数找匹配 ')' */
                    int depth = 1, t2, found = 0;
                    CValue cv2;
                    const int *r = p;
                    while (*r) {
                        TOK_GET(&t2, &r, &cv2);
                        if (t2 == '(')
                            depth++;
                        else if (t2 == ')') {
                            if (--depth == 0) {
                                /* 匹配 ')' 后是否 '{' */
                                if (*r == '{')
                                    found = 1;
                                break;
                            }
                        }
                    }
                    if (found) {
                        md->name = prev;
                        break;
                    }
                }
                prev = t;
            }
            if (md->name < TOK_IDENT)
                expect("function name");
        }
    } else {
        if (tok != '{')
            expect("'{'");
        md->body = tok_str_alloc();
        model_collect_body(md->body);
        /* 类型参数名不得与成员名重复: 实例化替换会把成员名也换掉。
           特征: 参数名 token 后跟 ';' ',' ':' '[' '=' 之一 = 成员名位置 */
        {
            int *toks = NULL, n = 0, i;
            const int *p = md->body->str;
            while (*p) {
                int t;
                CValue cv;
                TOK_GET(&t, &p, &cv);
                toks = tcc_realloc(toks, (n + 1) * sizeof(int));
                toks[n++] = t;
            }
            for (i = 0; i < n - 1; i++) {
                int j;
                if (toks[i + 1] != ';' && toks[i + 1] != ',' &&
                    toks[i + 1] != ':' && toks[i + 1] != '[' &&
                    toks[i + 1] != '=')
                    continue;
                for (j = 0; j < md->nparams; j++) {
                    if (toks[i] == md->params[j])
                        tcc_error("type parameter '%s' conflicts with member name",
                                  get_tok_str(md->params[j], NULL));
                }
            }
            tcc_free(toks);
        }
    }
    md->next = model_list;
    model_list = md;
    /* 函数定义行尾无分号 (C 函数定义合法), struct/union 定义带 ';'。
       统一吞掉可选 ';', 返回后 decl() 直接继续下一条声明 */
    if (tok == ';')
        next();
}

/* 实参 token 流 → 合成内部名 (Name_Arg1_Arg2...), 非字母数字转 '_' */
static int model_synth_name(ModelDef *md, TokenString **args)
{
    CString name;
    int i;

    cstr_new(&name);
    {
        const char *s0 = get_tok_str(md->name, NULL);
        for (; *s0; s0++)
            cstr_ccat(&name, *s0);
    }
    for (i = 0; i < md->nparams; i++) {
        const int *p = args[i]->str;
        int t;
        CValue cv;
        cstr_ccat(&name, '_');
        if (md->param_kind[i] == MODEL_PARAM_CONST) {
            /* 常量参数: 用值本身合名 (去后缀), 使等价常量写法共享缓存.
               按声明的有符号性区分负值/大无符号值进制格式 */
            char buf[24], *q;
            int is_signed = (md->const_tok[i] == TOK_CINT ||
                             md->const_tok[i] == TOK_CLLONG);
            TOK_GET(&t, &p, &cv);
            if (is_signed)
                snprintf(buf, sizeof buf, "%lld", (long long)(int64_t)cv.i);
            else
                snprintf(buf, sizeof buf, "%llu", (unsigned long long)cv.i);
            for (q = buf; *q; q++)
                cstr_ccat(&name, *q);
            continue;
        }
        while (*p) {
            const char *str;
            const char *q;
            TOK_GET(&t, &p, &cv);
            if (t == TOK_LINENUM)
                continue;   /* 行号标记不参与合成名 */
            str = get_tok_str(t, &cv);
            for (q = str; *q; q++) {
                char c = *q;
                if (!isalnum((unsigned char)c) && c != '_')
                    c = '_';
                cstr_ccat(&name, c);
            }
        }
    }
    cstr_ccat(&name, 0);
    return tok_alloc_const(name.data);
}

/* 常量参数实参归一化求值: 把 args[i] 的原始 token 流换成单个带符号后缀的
   值 token (2+2 -> 3), 使等价常量写法的实例共享同一缓存. 通过 begin_macro
   重放实参流调 expr_const64 (只读, 不修改实参流). tok 停在实参列表 ')' 之后 */
static void model_normalize_const_args(ModelDef *md, TokenString **args)
{
    int i;
    for (i = 0; i < md->nparams; i++) {
        if (md->param_kind[i] != MODEL_PARAM_CONST)
            continue;
        {
            int64_t v;
            TokenString *n, *replay;
            CValue cv;
            const int *save_mptr = macro_ptr;
            int save_tok = tok;
            CValue save_tokc = tokc;
            /* 重放求常量: 复制实参流并在末尾补一个行内 ';' (expr 的天然终止
               符) 再补 0, 使 expr_const64 求完值后读到的是一致都停在流内的
               ';', 而不是越过宏边界读外层 token. 显式 end_macro 弹栈;
               save/restore 保证外层解析位置完全不被扰动 */
            replay = tok_str_alloc();
            {
                const int *p = args[i]->str;
                int t;
                CValue cv2;
                while (*p) {
                    TOK_GET(&t, &p, &cv2);
                    tok_str_add2(replay, t, &cv2);
                }
                tok_str_add(replay, ';');
                tok_str_add(replay, 0);
            }
            begin_macro(replay, 0);
            next();
            v = expr_const64();
            end_macro();
            tok_str_free(replay);
            macro_ptr = save_mptr;
            tok = save_tok;
            tokc = save_tokc;
            tok_str_free(args[i]);
            n = tok_str_alloc();
            cv.i = (uint64_t)v;   /* 值放 64 位宽 (int/ll/uint 常量同槽) */
            tok_str_add2(n, md->const_tok[i], &cv);
            tok_str_add(n, 0);
            args[i] = n;
        }
    }
}

/* 实例化: tok 在 '(' 上。记录实参原始 token (顶层逗号分隔),
   合成内部名, 缓存查重, 未命中则替换 body 并重放 struct_decl */
static void model_instantiate(CType *type, ModelDef *md)
{
    TokenString **args;
    TokenString *ts;
    Sym *s;
    int i, synth, depth;

    args = tcc_malloc(md->nparams * sizeof *args);
    for (i = 0; i < md->nparams; i++)
        args[i] = tok_str_alloc();
    next();   /* '(' */
    /* 记录实参原始 token: 括号深度计数支持嵌套实例化 Box(double),
       顶层逗号分隔多个实参 */
    i = 0;
    depth = 0;
    for (;;) {
        if (tok == '(')
            depth++;
        else if (tok == ')') {
            if (depth == 0)
                break;   /* 实参列表结束 */
            depth--;
        } else if (tok == ',' && depth == 0) {
            tok_str_add(args[i], 0);   /* 0 结尾: 后续遍历靠 0 停止 */
            i++;
            if (i >= md->nparams)
                tcc_error("too many type arguments for model '%s'",
                          get_tok_str(md->name, NULL));
            next();
            continue;
        } else if (tok == TOK_EOF) {
            expect("'('");
        }
        tok_str_add_tok(args[i]);
        next();
    }
    tok_str_add(args[i], 0);   /* 0 结尾: 后续遍历靠 0 停止 */
    next();   /* ')' */
    if (i != md->nparams - 1)
        tcc_error("wrong number of type arguments for model '%s'",
                  get_tok_str(md->name, NULL));
    /* 常量参数归一化求值 (2+2 -> 3, 等价写法共享缓存) */
    model_normalize_const_args(md, args);
    /* 缓存查重: 合成名已定义过 → 直接复用 */
    synth = model_synth_name(md, args);
    s = struct_find(synth);
    if (s && (s->type.t & ~VT_BTYPE) == (md->kind & ~VT_BTYPE) && s->c != -1) {
        type->t = s->type.t;
        type->ref = s;
        tcc_free(args);
        return;
    }
    /* 未命中: 复制 body, 类型参数 token → 实参 token 序列 */
    ts = tok_str_alloc();
    {
        CValue cv;
        cv.i = synth;
        tok_str_add2(ts, md->kind == VT_STRUCT ? TOK_STRUCT : TOK_UNION, &cv);
        tok_str_add2(ts, synth, &cv);
    }
    {
        const int *p = md->body->str;
        int t;
        CValue cv;
        while (*p) {
            TOK_GET(&t, &p, &cv);
            for (i = 0; i < md->nparams; i++) {
                if (t == md->params[i]) {
                    /* 替换为实参 token 序列 */
                    const int *q = args[i]->str;
                    int t2;
                    CValue cv2;
                    while (*q) {
                        TOK_GET(&t2, &q, &cv2);
                        tok_str_add2(ts, t2, &cv2);
                    }
                    t = 0;
                    break;
                }
            }
            if (t)
                tok_str_add2(ts, t, &cv);
        }
        /* 缓冲 token: struct_decl 内部 skip('}') 后不再 next(),
           流以 ';' 收尾, end_macro() 显式弹出 (方法实现同款) */
        cv.i = 0;
        tok_str_add2(ts, ';', &cv);
    }
    /* 重放: struct 合成名 { 替换后成员 }; 走标准 struct_decl 布局。
       struct_decl 内嵌方法时 method_emit_all 会改写局部作用域
       (gen_function 同款), 完整保存/恢复 (与 method_emit_all 一致) */
    {
        int save_tok = tok;
        CValue save_tokc = tokc;
        struct scope *save_scope = cur_scope;
        struct scope *save_root = root_scope;
        Sym *save_lstack = local_stack;
        int save_lscope = local_scope;
        Section *save_section = cur_text_section;
        int save_nocode = nocode_wanted;
        begin_macro(ts, 1);
        next();   /* struct/union */
        /* 隔离 local_stack: 局部作用域实例化时 struct 字段符号若进
           local_stack 会随函数结束失效, 方法符号跨函数调用变 UND。
           文件级 struct 定义 (字段符号进 global_stack) 本就该在
           干净环境重放 */
        local_stack = NULL;
        struct_decl(type, md->kind);
        local_stack = save_lstack;
        end_macro();
        tok = save_tok;
        tokc = save_tokc;
        cur_scope = save_scope;
        root_scope = save_root;
        local_stack = save_lstack;
        local_scope = save_lscope;
        cur_text_section = save_section;
        nocode_wanted = save_nocode;
    }
    tcc_free(args);
}

/* ------------------------------------------------------------------------- */
/* model function 泛型 (函数模板, 实例化调用)                                 */

/* 函数实例缓存: 合成名 → 内部函数符号 + 函数体 token 流 (延迟编译) */
typedef struct ModelFn {
    struct ModelFn *next;
    int name;              /* 合成内部函数名 tok (max2_int) */
    Sym *sym;              /* 内部函数符号 */
    TokenString *body;     /* 函数体 token 流 ({...} 替换后, 含 ';' 缓冲) */
} ModelFn;

static ModelFn *model_fn_list;   /* 待编译函数实例表 (兼缓存) */

static ModelFn *model_fn_find(int name)
{
    ModelFn *fn;
    for (fn = model_fn_list; fn; fn = fn->next)
        if (fn->name == name)
            return fn;
    return NULL;
}

/* 解析替换后的函数签名 (返回类型 + 函数名 + 参数表), 注册内部函数符号。
   ts 为完整函数定义流 (签名 + 函数体), type_decl 停在 '{' 前 */
static void model_fn_register(TokenString *ts, ModelFn *fn)
{
    CType type, btype;
    AttributeDef ad, adbase;
    int v;
    Sym *save_lstack = local_stack;
    int save_tok = tok;
    CValue save_tokc = tokc;

    /* 参数符号必须进 global_stack (隔离局部作用域, 方法同款) */
    local_stack = NULL;
    begin_macro(ts, 1);
    next();   /* 返回类型首 token */
    memset(&adbase, 0, sizeof adbase);
    if (!parse_btype(&btype, &adbase, 0))
        expect("return type");
    type = btype;
    ad = adbase;
    type_decl(&type, &ad, &v, TYPE_DIRECT);   /* v = 合成名, type = VT_FUNC */
    end_macro();
    local_stack = save_lstack;
    tok = save_tok;
    tokc = save_tokc;
    fn->sym = external_sym(v, &type, 0, &ad);
}

/* 实例化 model function 调用: tok 在 '(' 上 (类型实参列表)。
   成功 → vtop = 内部函数符号, tok = 调用参数 '('; 返回 1 */
static int model_function_call(int name, ModelDef *md)
{
    TokenString **args;
    TokenString *ts, *body;
    ModelFn *fn;
    Sym *s;
    int i, synth, depth;

    /* 1. 记录实参原始 token (顶层逗号分隔, 括号深度计数) */
    args = tcc_malloc(md->nparams * sizeof *args);
    for (i = 0; i < md->nparams; i++)
        args[i] = tok_str_alloc();
    next();   /* '(' */
    i = 0;
    depth = 0;
    for (;;) {
        if (tok == '(')
            depth++;
        else if (tok == ')') {
            if (depth == 0)
                break;
            depth--;
        } else if (tok == ',' && depth == 0) {
            tok_str_add(args[i], 0);
            i++;
            if (i >= md->nparams)
                tcc_error("too many type arguments for model function '%s'",
                          get_tok_str(md->name, NULL));
            next();
            continue;
        } else if (tok == TOK_EOF) {
            expect("'('");
        }
        tok_str_add_tok(args[i]);
        next();
    }
    tok_str_add(args[i], 0);
    next();   /* ')' */
    if (i != md->nparams - 1)
        tcc_error("wrong number of type arguments for model function '%s'",
                  get_tok_str(md->name, NULL));
    /* 常量参数归一化求值 (2+2 -> 3, 等价写法共享缓存) */
    model_normalize_const_args(md, args);
    /* 2. 合成内部名 + 缓存查重 */
    synth = model_synth_name(md, args);
    fn = model_fn_find(synth);
    if (!fn) {
        /* 3. 未实例化: 构造完整定义流 (类型参数 → 实参, 函数名 → 合成名) */
        ts = tok_str_alloc();
        {
            const int *p = md->body->str;
            int t;
            CValue cv;
            while (*p) {
                TOK_GET(&t, &p, &cv);
                if (t == md->name) {
                    /* 函数名 → 合成内部名 */
                    cv.i = synth;
                    tok_str_add2(ts, synth, &cv);
                    continue;
                }
                for (i = 0; i < md->nparams; i++) {
                    if (t == md->params[i]) {
                        /* 类型参数 → 实参 token 序列 */
                        const int *q = args[i]->str;
                        int t2;
                        CValue cv2;
                        while (*q) {
                            TOK_GET(&t2, &q, &cv2);
                            tok_str_add2(ts, t2, &cv2);
                        }
                        t = 0;
                        break;
                    }
                }
                if (t)
                    tok_str_add2(ts, t, &cv);
            }
            /* 缓冲: 签名解析后 type_decl 停在 '{', end_macro 弹出 */
            cv.i = 0;
            tok_str_add2(ts, 0, &cv);
        }
        /* 4. 注册符号 + 提取函数体流 (延迟编译)。
           注意: 先提取函数体 (model_fn_register 的 begin_macro(ts,1)+
           end_macro 会 tok_str_free 释放 ts, 之后 ts->str 悬空!) */
        fn = tcc_malloc(sizeof *fn);
        memset(fn, 0, sizeof *fn);
        fn->name = synth;
        /* 函数体流: '{' 到匹配 '}' (含) + ';' 缓冲 */
        body = tok_str_alloc();
        {
            const int *p = ts->str;
            int t, depth2 = 0, started = 0;
            CValue cv;
            for (;;) {
                TOK_GET(&t, &p, &cv);
                if (t == 0)
                    break;   /* 流尾 */
                if (t == '{') {
                    depth2++;
                    started = 1;
                } else if (t == '}' && started) {
                    depth2--;
                    if (depth2 == 0) {
                        tok_str_add2(body, t, &cv);
                        break;
                    }
                }
                if (started)
                    tok_str_add2(body, t, &cv);
            }
            cv.i = 0;
            tok_str_add2(body, ';', &cv);   /* gen_function 末尾 next() 缓冲 */
        }
        fn->body = body;
        model_fn_register(ts, fn);   /* 之后 ts 被释放, 不再使用 */
        fn->next = model_fn_list;
        model_fn_list = fn;
        tcc_free(args);
    } else {
        tcc_free(args);
    }
    /* 5. vtop = 内部函数符号; tok 已停在调用参数 '(' 上 */
    s = fn->sym;
    vset(&s->type, s->r, s->c);
    vtop->sym = s;
    if (s->r & VT_SYM)
        vtop->c.i = 0;
    return 1;
}

/* 文件末尾: 编译所有待编译函数实例 (与 method_compile_all 同款) */
static void model_fn_compile_all(void)
{
    ModelFn *fn;
    for (fn = model_fn_list; fn; fn = fn->next) {
        int save_tok = tok;
        CValue save_tokc = tokc;
        begin_macro(fn->body, 1);
        next();   /* '{' */
        cur_text_section = text_section;
        gen_function(fn->sym);
        end_macro();
        tok = save_tok;
        tokc = save_tokc;
    }
}

/* enum/struct/union declaration. u is VT_ENUM/VT_STRUCT/VT_UNION */
static void struct_decl(CType *type, int u)
{
    int v, c, size, align, flexible;
    int bit_size, bsize, bt, ut;
    Sym *s, *ss, **ps;
    AttributeDef ad, ad1;
    CType type1, btype;


    memset(&ad, 0, sizeof ad);
    next();
    parse_attribute(&ad);

    v = 0;
    if (tok >= TOK_IDENT) /* struct/enum tag */
        v = tok, next();

    bt = ut = 0;
    if (u == VT_ENUM) {
        ut = VT_INT;
        if (tok == ':') { /* C2x enum : <type> ... */
            next();
            if (!parse_btype(&btype, &ad1, 0)
             || !is_integer_btype(btype.t & VT_BTYPE))
                expect("enum type");
            bt = ut = btype.t & (VT_BTYPE|VT_LONG|VT_UNSIGNED|VT_DEFSIGN);
        }
    }

    if (v) {
        /* struct already defined ? return it */
        s = struct_find(v);
        if (s && (s->sym_scope == local_scope || (tok != '{' && tok != ';'))) {
            if (u == s->type.t)
                goto do_decl;
            if (u == VT_ENUM && IS_ENUM(s->type.t)) /* XXX: check integral types */
                goto do_decl;
            tcc_error("redeclaration of '%s'", get_tok_str(v, NULL));
        }
    } else {
        if (tok != '{')
            expect("struct/union/enum name");
        v = anon_sym++;
    }
    /* Record the original enum/struct/union token.  */
    type1.t = u | ut;
    type1.ref = NULL;
    /* we put an undefined size for struct/union */
    s = sym_push(v | SYM_STRUCT, &type1, 0, bt ? 0 : -1);
    s->r = 0; /* default alignment is zero as gcc */
do_decl:
    type->t = s->type.t;
    type->ref = s;

    if (tok == '{') {
        next();
        if (s->c != -1
            && !(u == VT_ENUM && s->c == 0)) /* not yet defined typed enum */
            tcc_error("struct/union/enum already defined");
        s->c = -2;
        /* cannot be empty */
        /* non empty enums are not allowed */
        ps = &s->next;
        if (u == VT_ENUM) {
            long long ll = 0, pl = 0, nl = 0;
	    CType t;
            t.ref = s;
            s->sym_scope = local_scope; /* anonymous symbol won't have set */
            /* enum symbols have static storage */
            t.t = VT_INT|VT_STATIC|VT_ENUM_VAL;
            if (bt)
                t.t = bt|VT_STATIC|VT_ENUM_VAL;
            for(;;) {
                v = tok;
                if (v < TOK_UIDENT)
                    expect("identifier");
                next();
                if (tok == '=') {
                    next();
		    ll = expr_const64();
                }
                if (bt && !in_range(ll, t.t))
		    tcc_error("enumerator '%s' out of range of its type",
			get_tok_str(v, NULL));
                ss = sym_push(v, &t, VT_CONST, 0);
                ss->enum_val = ll;
                *ps = ss, ps = &ss->next;
                if (ll < nl)
                    nl = ll;
                if (ll > pl)
                    pl = ll;
                if (tok != ',')
                    break;
                next();
                ll++;
                /* NOTE: we accept a trailing comma */
                if (tok == '}')
                    break;
            }
            skip('}');

            if (bt) {
                t.t = bt;
                s->c = 2;
                goto enum_done;
            }

            /* set integral type of the enum */
            t.t = VT_INT;
            if (nl >= 0) {
                if (pl != (unsigned)pl)
                    t.t = (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);
                t.t |= VT_UNSIGNED;
            } else if (pl != (int)pl || nl != (int)nl)
                t.t = (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);

            /* set type for enum members */
            for (ss = s->next; ss; ss = ss->next) {
                ll = ss->enum_val;
                if (ll == (int)ll) /* default is int if it fits */
                    continue;
                if (t.t & VT_UNSIGNED) {
                    ss->type.t |= VT_UNSIGNED;
                    if (ll == (unsigned)ll)
                        continue;
                }
                ss->type.t = (ss->type.t & ~VT_BTYPE)
                    | (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);
            }
            s->c = 1;
        enum_done:
            s->type.t = type->t = t.t | VT_ENUM;

        } else {
            c = 0;
            flexible = 0;
            while (tok != '}') {
                if (!parse_btype(&btype, &ad1, 0)) {
                    if (tok == TOK_STATIC_ASSERT) {
                        do_Static_assert();
                        continue;
                    }
		    skip(';');
		    continue;
		}
                while (1) {
		    if (flexible)
		        tcc_error("flexible array member '%s' not at the end of struct",
                              get_tok_str(v, NULL));
                    bit_size = -1;
                    v = 0;
                    type1 = btype;
                    if (tok != ':') {
			if (tok != ';')
                            type_decl(&type1, &ad1, &v, TYPE_DIRECT);
                        if ((type1.t & VT_BTYPE) == VT_FUNC) {
                            /* C++ 式方法: struct 体内函数定义 */
                            if (tok != '{')
                                tcc_error("method '%s' needs a body",
                                          get_tok_str(v, NULL));
                            method_parse(s, v, &type1);
                            if (tok == ';')   /* 方法后可选 ';' */
                                next();
                            goto method_member_done;
                        }
                        if (v == 0) {
                    	    if ((type1.t & VT_BTYPE) != VT_STRUCT)
                        	expect("identifier");
                    	    else {
				int v = btype.ref->v;
				if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
				    if (tcc_state->ms_extensions == 0)
                        		expect("identifier");
				}
                    	    }
                        }
                        if (type_size(&type1, &align) < 0) {
			    if ((u == VT_STRUCT) && (type1.t & VT_ARRAY) && c)
			        flexible = 1;
			    else
			        tcc_error("field '%s' has incomplete type",
                                      get_tok_str(v, NULL));
                        }
                        if ((type1.t & VT_BTYPE) == VT_FUNC ||
			    (type1.t & VT_BTYPE) == VT_VOID ||
                            (type1.t & VT_STORAGE))
                            tcc_error("invalid type for '%s'", 
                                  get_tok_str(v, NULL));
                    }
                    if (tok == ':') {
                        next();
                        bit_size = expr_const();
                        /* XXX: handle v = 0 case for messages */
                        if (bit_size < 0)
                            tcc_error("negative width in bit-field '%s'", 
                                  get_tok_str(v, NULL));
                        if (v && bit_size == 0)
                            tcc_error("zero width for bit-field '%s'", 
                                  get_tok_str(v, NULL));
			parse_attribute(&ad1);
                    }
                    size = type_size(&type1, &align);
                    if (bit_size >= 0) {
                        bt = type1.t & VT_BTYPE;
                        if (bt != VT_INT && 
                            bt != VT_BYTE && 
                            bt != VT_SHORT &&
                            bt != VT_BOOL &&
                            bt != VT_LLONG)
                            tcc_error("bitfields must have scalar type");
                        bsize = size * 8;
                        if (bit_size > bsize) {
                            tcc_error("width of '%s' exceeds its type",
                                  get_tok_str(v, NULL));
                        } else if (bit_size == bsize
				    && !*tcc_state->pack_stack_ptr
                                    && !ad.a.packed && !ad1.a.packed) {
                            /* no need for bit fields */
                            ;
                        } else if (bit_size == 64) {
                            tcc_error("field width 64 not implemented");
                        } else {
                            type1.t = (type1.t & ~VT_STRUCT_MASK)
                                | VT_BITFIELD
                                | ((unsigned)bit_size << (VT_STRUCT_SHIFT + 6));
                        }
                    }
                    if (v != 0 || (type1.t & VT_BTYPE) == VT_STRUCT) {
                        /* Remember we've seen a real field to check
			   for placement of flexible array member. */
			c = 1;
                    }
		    /* If member is a struct or bit-field, enforce
		       placing into the struct (as anonymous).  */
                    if (v == 0 &&
			((type1.t & VT_BTYPE) == VT_STRUCT ||
			 bit_size >= 0)) {
		        v = anon_sym++;
		    }
                    if (v) {
                        ss = sym_push(v | SYM_FIELD, &type1, 0, 0);
                        ss->a = ad1.a;
                        *ps = ss;
                        ps = &ss->next;
                    }
                    if (tok == ';' || tok == TOK_EOF)
                        break;
                    skip(',');
                }
                skip(';');
method_member_done: ;
            }
            skip('}');
	    parse_attribute(&ad);
            if (ad.cleanup_func) {
                tcc_warning("attribute '__cleanup__' ignored on type");
            }
	    check_fields(type, 1);
	    check_fields(type, 0);
            struct_layout(type, &ad);
            method_emit_all(s);
        }
        if (debug_modes)
            tcc_debug_fix_forw(tcc_state, type);
    }
}

static void sym_to_attr(AttributeDef *ad, Sym *s)
{
    merge_symattr(&ad->a, &s->a);
    merge_funcattr(&ad->f, &s->f);
}

/* Add type qualifiers to a type. If the type is an array then the qualifiers
   are added to the element type, copied because it could be a typedef. */
static void parse_btype_qualify(CType *type, int qualifiers)
{
    while (type->t & VT_ARRAY) {
        type->ref = sym_push(SYM_FIELD, &type->ref->type, 0, type->ref->c);
        type = &type->ref->type;
    }
    type->t |= qualifiers;
}

/* return 0 if no type declaration. otherwise, return the basic type
   and skip it. 
 */
static int parse_btype(CType *type, AttributeDef *ad, int ignore_label)
{
    int t, u, bt, st, type_found, typespec_found, g, n;
    Sym *s;
    CType type1;

    memset(ad, 0, sizeof(AttributeDef));
    type_found = 0;
    typespec_found = 0;
    t = VT_INT;
    bt = st = -1;
    type->ref = NULL;

    while(1) {
        switch(tok) {
        case TOK_MODEL:
            /* model 泛型定义: model struct/union Name(T...) {...} /
               model (T) 返回类型 Name(参数) {...} (函数泛型) */
            next();
            if (tok != TOK_STRUCT && tok != TOK_UNION && tok != '(')
                tcc_error("'model' requires 'struct', 'union' or '('");
            model_decl();
            model_decl_seen = 1;
            return 0;   /* 不生成类型, 由调用方继续解析后续声明 */
        case TOK_EXTENSION:
            /* currently, we really ignore extension */
            next();
            continue;

            /* basic types */
        case TOK_CHAR:
            u = VT_BYTE;
        basic_type:
            next();
        basic_type1:
            if (u == VT_SHORT || u == VT_LONG) {
                if (st != -1 || (bt != -1 && bt != VT_INT))
                    tmbt: tcc_error("too many basic types");
                st = u;
            } else {
                if (bt != -1 || (st != -1 && u != VT_INT))
                    goto tmbt;
                if ((t & VT_DEFSIGN) && (u == VT_VOID || u > VT_LLONG))
                    goto tmbt;
                bt = u;
            }
            if (u != VT_INT)
                t = (t & ~(VT_BTYPE|VT_LONG)) | u;
            typespec_found = 1;
            break;
        case TOK_VOID:
            u = VT_VOID;
            goto basic_type;
        case TOK_SHORT:
            u = VT_SHORT;
            goto basic_type;
        case TOK_INT:
            u = VT_INT;
            goto basic_type;
        case TOK_ALIGNAS:
            { int n;
              AttributeDef ad1;
              next();
              skip('(');
              memset(&ad1, 0, sizeof(AttributeDef));
              if (parse_btype(&type1, &ad1, 0)) {
                  type_decl(&type1, &ad1, &n, TYPE_ABSTRACT);
                  if (ad1.a.aligned)
                    n = 1 << (ad1.a.aligned - 1);
                  else
                    type_size(&type1, &n);
              } else {
                  n = expr_const();
                  if (n < 0 || (n & (n - 1)) != 0)
                    tcc_error("alignment must be a positive power of two");
              }
              skip(')');
              ad->a.aligned = exact_log2p1(n);
            }
            continue;
        case TOK_LONG:
            if ((t & VT_BTYPE) == VT_DOUBLE) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LDOUBLE;
            } else if ((t & (VT_BTYPE|VT_LONG)) == VT_LONG) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LLONG;
            } else {
                u = VT_LONG;
                goto basic_type;
            }
            next();
            break;
        case TOK_BOOL:
            u = VT_BOOL;
            goto basic_type;
        case TOK_COMPLEX:
            tcc_error("_Complex is not yet supported");
        case TOK_FLOAT:
            u = VT_FLOAT;
            goto basic_type;
        case TOK_DOUBLE:
            if ((t & (VT_BTYPE|VT_LONG)) == VT_LONG) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LDOUBLE;
            } else {
                u = VT_DOUBLE;
                goto basic_type;
            }
            next();
            break;
        case TOK_ENUM:
            struct_decl(&type1, VT_ENUM);
        basic_type2:
            u = type1.t;
            type->ref = type1.ref;
            goto basic_type1;
        case TOK_STRUCT:
            struct_decl(&type1, VT_STRUCT);
            goto basic_type2;
        case TOK_UNION:
            struct_decl(&type1, VT_UNION);
            goto basic_type2;

            /* type modifiers */
        case TOK__Atomic:
            next();
            type->t = t;
            parse_btype_qualify(type, VT_ATOMIC);
            t = type->t;
            if (tok == '(') {
                parse_expr_type(&type1);
                /* remove all storage modifiers except typedef */
                type1.t &= ~(VT_STORAGE&~VT_TYPEDEF);
                if (type1.ref)
                    sym_to_attr(ad, type1.ref);
                goto basic_type2;
            }
            break;
        case TOK_CONST1:
        case TOK_CONST2:
        case TOK_CONST3:
            type->t = t;
            parse_btype_qualify(type, VT_CONSTANT);
            t = type->t;
            next();
            break;
        case TOK_VOLATILE1:
        case TOK_VOLATILE2:
        case TOK_VOLATILE3:
            type->t = t;
            parse_btype_qualify(type, VT_VOLATILE);
            t = type->t;
            next();
            break;
        case TOK_SIGNED1:
        case TOK_SIGNED2:
        case TOK_SIGNED3:
            if ((t & (VT_DEFSIGN|VT_UNSIGNED)) == (VT_DEFSIGN|VT_UNSIGNED))
                tcc_error("signed and unsigned modifier");
            t |= VT_DEFSIGN;
            next();
            typespec_found = 1;
            break;
        case TOK_REGISTER:
        case TOK_AUTO:
        case TOK_RESTRICT1:
        case TOK_RESTRICT2:
        case TOK_RESTRICT3:
            next();
            break;
        case TOK_UNSIGNED:
            if ((t & (VT_DEFSIGN|VT_UNSIGNED)) == VT_DEFSIGN)
                tcc_error("signed and unsigned modifier");
            t |= VT_DEFSIGN | VT_UNSIGNED;
            next();
            typespec_found = 1;
            break;

            /* storage */
        case TOK_EXTERN:
            g = VT_EXTERN;
            goto storage;
        case TOK_STATIC:
            g = VT_STATIC;
            goto storage;
        case TOK_TYPEDEF:
            g = VT_TYPEDEF;
            goto storage;
       storage:
            if (t & (VT_EXTERN|VT_STATIC|VT_TYPEDEF) & ~g)
                tcc_error("multiple storage classes");
            t |= g;
            next();
            break;
        case TOK_INLINE1:
        case TOK_INLINE2:
        case TOK_INLINE3:
            t |= VT_INLINE;
            next();
            break;
        case TOK_NORETURN3:
            next();
            ad->f.func_noreturn = 1;
            break;
            /* GNUC attribute */
        case TOK_ATTRIBUTE1:
        case TOK_ATTRIBUTE2:
            parse_attribute(ad);
            if (ad->attr_mode) {
                u = ad->attr_mode -1;
                t = (t & ~(VT_BTYPE|VT_LONG)) | u;
            }
            continue;
            /* GNUC typeof */
        case TOK_TYPEOF1:
        case TOK_TYPEOF2:
        case TOK_TYPEOF3:
            next();
            parse_expr_type(&type1);
            /* remove all storage modifiers except typedef */
            type1.t &= ~(VT_STORAGE&~VT_TYPEDEF);
            if (type1.ref) {
                sym_to_attr(ad, type1.ref);
                if (type1.t & VT_ARRAY)
                    type1.t |= VT_BT_ARRAY;
            }
            goto basic_type2;
        case TOK_THREAD_LOCAL:
        case TOK___thread:
            if (t & VT_TLS)
                tcc_error("multiple thread-local storage specifiers");
            t |= VT_TLS;
            next();
            break;
        default:
            if (typespec_found)
                goto the_end;
            /* model struct/union 泛型实例化: Name(类型实参...) → 合成类型。
               function 泛型不在此 (unary 调用点处理) */
            if (model_find(tok) && model_find(tok)->kind != VT_FUNC) {
                ModelDef *md = model_find(tok);
                n = tok, next();
                if (tok == '(') {
                    model_instantiate(&type1, md);
                    u = type1.t;
                    type->ref = type1.ref;
                    goto basic_type2;
                }
                unget_tok(n);
                goto the_end;
            }
            s = sym_find(tok);
            if (!s || !(s->type.t & VT_TYPEDEF))
                goto the_end;

            n = tok, next();
            if (tok == ':' && ignore_label) {
                /* ignore if it's a label */
                unget_tok(n);
                goto the_end;
            }

            t &= ~(VT_BTYPE|VT_LONG);
            u = t & ~(VT_CONSTANT | VT_VOLATILE), t ^= u;
            type->t = (s->type.t & ~VT_TYPEDEF) | u;
            type->ref = s->type.ref;
            if (t)
                parse_btype_qualify(type, t);
            t = type->t;
            if (t & VT_ARRAY)
                t |= VT_BT_ARRAY;
            /* get attributes from typedef */
            sym_to_attr(ad, s);
            typespec_found = 1;
            st = bt = -2;
            break;
        }
        type_found = 1;
    }
the_end:
    if (tcc_state->char_is_unsigned) {
        if ((t & (VT_DEFSIGN|VT_BTYPE)) == VT_BYTE)
            t |= VT_UNSIGNED;
    }
    /* VT_LONG is used just as a modifier for VT_INT / VT_LLONG */
    bt = t & (VT_BTYPE|VT_LONG);
    if (bt == VT_LONG)
        t |= LONG_SIZE == 8 ? VT_LLONG : VT_INT;
#ifdef TCC_USING_DOUBLE_FOR_LDOUBLE
    if (bt == VT_LDOUBLE)
        t = (t & ~(VT_BTYPE|VT_LONG)) | (VT_DOUBLE|VT_LONG);
#endif
    type->t = t;
    return type_found;
}

/* convert a function parameter type (array to pointer and function to
   function pointer) */
static inline void convert_parameter_type(CType *pt)
{
    /* remove const and volatile qualifiers (XXX: const could be used
       to indicate a const function parameter */
    pt->t &= ~(VT_CONSTANT | VT_VOLATILE);
    /* array must be transformed to pointer according to ANSI C */
    pt->t &= ~(VT_ARRAY | VT_VLA);
    if ((pt->t & VT_BTYPE) == VT_FUNC) {
        mk_pointer(pt);
    }
}

ST_FUNC CString* parse_asm_str(void)
{
    skip('(');
    return parse_mult_str("string constant");
}

/* Parse an asm label and return the token */
static int asm_label_instr(void)
{
    int v;
    char *astr;

    next();
    astr = parse_asm_str()->data;
    skip(')');
#ifdef ASM_DEBUG
    printf("asm_alias: \"%s\"\n", astr);
#endif
    v = tok_alloc_const(astr);
    return v;
}

static int post_type(CType *type, AttributeDef *ad, int storage, int td)
{
    int n, l, t1, arg_size, align;
    Sym **plast, *s, *first, **ps, *sr;
    AttributeDef ad1;
    CType pt;
    TokenString *vla_array_tok = NULL;
    int *vla_array_str = NULL;

    if (tok == '(') {
        /* function type, or recursive declarator (return if so) */
        next();
	if (TYPE_DIRECT == (td & (TYPE_DIRECT|TYPE_ABSTRACT)))
	  return 0;

        /* we push a anonymous symbol which will contain the function prototype */
        /* it also serves as a boundary for the function parameter scope */
        ps = local_stack ? &local_stack : &global_stack;
        ++local_scope;
        sr = sym_push2(ps, SYM_FIELD, 0, 0);

	if (tok == ')')
	  l = 0;
	else if (parse_btype(&pt, &ad1, 0))
	  l = FUNC_NEW;
	else if (td & (TYPE_DIRECT|TYPE_ABSTRACT)) {
            sym_pop(ps, sr->prev, 0);
            --local_scope;
	    merge_attr (ad, &ad1);
	    return 0;
	} else
	  l = FUNC_OLD;

        first = NULL;
        plast = &first;
        arg_size = 0;
        if (l) {
            for(;;) {
                /* read param name and compute offset */
                if (l != FUNC_OLD) {
                    if ((pt.t & VT_BTYPE) == VT_VOID && tok == ')')
                        break;
                    type_decl(&pt, &ad1, &n, TYPE_DIRECT | TYPE_ABSTRACT | TYPE_PARAM);
                    if ((pt.t & VT_BTYPE) == VT_VOID)
                        tcc_error("parameter declared as void");
                    if (n == 0 || (td & TYPE_PARAM))
                        n |= SYM_FIELD;
                } else {
                    n = tok;
                    pt.t = VT_INT | VT_EXTERN; /* default type */
                    pt.ref = NULL;
                    next();
                }
                if (n < TOK_UIDENT)
                    expect("identifier");
                convert_parameter_type(&pt);
                arg_size += (type_size(&pt, &align) + PTR_SIZE - 1) / PTR_SIZE;
                /* these symbols may be evaluated for VLArrays (see below, under
                   nocode_wanted) Example: int func(int a, int b[++a]); */
                s = sym_push(n, &pt, VT_LOCAL|VT_LVAL, 0);
                *plast = s;
                plast = &s->next;
                if (tok == ')')
                    break;
                skip(',');
                if (l == FUNC_NEW && tok == TOK_DOTS) {
                    l = FUNC_ELLIPSIS;
                    next();
                    break;
                }
		if (l == FUNC_NEW && !parse_btype(&pt, &ad1, 0))
		    tcc_error("invalid type");
            }
        } else
            /* if no parameters, then old type prototype */
            l = FUNC_OLD;
        skip(')');
        /* NOTE: const is ignored in returned type as it has a special
           meaning in gcc / C++ */
        type->t &= ~VT_CONSTANT; 
        /* some ancient pre-K&R C allows a function to return an array
           and the array brackets to be put after the arguments, such 
           that "int c()[]" means something like "int[] c()" */
        if (tok == '[') {
            next();
            skip(']'); /* only handle simple "[]" */
            mk_pointer(type);
        }
        ad->f.func_args = arg_size;
        ad->f.func_type = l;
        sr->type = *type, s = sr;
        s->a = ad->a;
        s->f = ad->f;
        s->next = first;
        type->t = VT_FUNC;
        type->ref = s;
        /* unlink parameter symbols from the token table, keep on stack */
        sym_pop(ps, sr, 1);
        --local_scope;

    } else if (tok == '[') {
	int saved_nocode_wanted = nocode_wanted;
        /* array definition */
        next();
        n = -1;
        t1 = 0;
        if (td & TYPE_PARAM) while (1) {
	    /* XXX The optional type-quals and static should only be accepted
	       in parameter decls.  The '*' as well, and then even only
	       in prototypes (not function defs).  */
	    switch (tok) {
	    case TOK_RESTRICT1: case TOK_RESTRICT2: case TOK_RESTRICT3:
	    case TOK_CONST1:
	    case TOK_VOLATILE1:
	    case TOK_STATIC:
	    case '*':
		next();
		continue;
	    default:
		break;
	    }
            if (tok != ']') {
		/* Code generation is not done now but has to be done
		   at start of function. Save code here for later use. */
	        nocode_wanted = 1;
		skip_or_save_block(&vla_array_tok);
		unget_tok(0);
		vla_array_str = vla_array_tok->str;
		begin_macro(vla_array_tok, 2);
		next();
	        gexpr();
		end_macro();
		next();
		goto check;
            }
            break;

	} else if (tok != ']') {
            if (!local_stack || (storage & VT_STATIC))
                vpushi(expr_const());
            else {
		/* VLAs (which can only happen with local_stack && !VT_STATIC)
		   length must always be evaluated, even under nocode_wanted,
		   so that its size slot is initialized (e.g. under sizeof
		   or typeof).  */
		nocode_wanted = 0;
		gexpr();
	    }
check:
            if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                n = vtop->c.i;
                if (n < 0)
                    tcc_error("invalid array size");
            } else {
                if (!is_integer_btype(vtop->type.t & VT_BTYPE))
                    tcc_error("size of variable length array should be an integer");
                n = 0;
                t1 = VT_VLA;
            }
        }
        skip(']');
        /* parse next post type */
        post_type(type, ad, storage, (td & ~(TYPE_DIRECT|TYPE_ABSTRACT)) | TYPE_NEST);

        if ((type->t & VT_BTYPE) == VT_FUNC)
            tcc_error("declaration of an array of functions");
        if ((type->t & VT_BTYPE) == VT_VOID
            || type_size(type, &align) < 0)
            tcc_error("declaration of an array of incomplete type elements");

        t1 |= type->t & VT_VLA;

        if (t1 & VT_VLA) {
            if (n < 0) {
		if  (td & TYPE_NEST)
                    tcc_error("need explicit inner array size in VLAs");
	    }
	    else {
                loc -= type_size(&int_type, &align);
                loc &= -align;
                n = loc;

                vpush_type_size(type, &align);
                gen_op('*');
                vset(&int_type, VT_LOCAL|VT_LVAL, n);
                vswap();
                vstore();
	    }
        }
        if (n != -1)
            vpop();
	nocode_wanted = saved_nocode_wanted;
                
        /* we push an anonymous symbol which will contain the array
           element type */
        s = sym_push(SYM_FIELD, type, 0, n);
        type->t = (t1 ? VT_VLA : VT_ARRAY) | VT_PTR;
        type->ref = s;

        if (vla_array_str) {
            /* for function args, the top dimension is converted to pointer */
	    if ((t1 & VT_VLA) && (td & TYPE_NEST))
	        s->vla_array_str = vla_array_str;
	    else
	        tok_str_free_str(vla_array_str);
	}
    }
    return 1;
}

/* Parse a type declarator (except basic type), and return the type
   in 'type'. 'td' is a bitmask indicating which kind of type decl is
   expected. 'type' should contain the basic type. 'ad' is the
   attribute definition of the basic type. It can be modified by
   type_decl().  If this (possibly abstract) declarator is a pointer chain
   it returns the innermost pointed to type (equals *type, but is a different
   pointer), otherwise returns type itself, that's used for recursive calls.  */
static CType *type_decl(CType *type, AttributeDef *ad, int *v, int td)
{
    CType *post, *ret;
    int qualifiers, storage;

    /* recursive type, remove storage bits first, apply them later again */
    storage = type->t & VT_STORAGE;
    type->t &= ~VT_STORAGE;
    post = ret = type;

    while (tok == '*') {
        qualifiers = 0;
    redo:
        next();
        switch(tok) {
        case TOK__Atomic:
            qualifiers |= VT_ATOMIC;
            goto redo;
        case TOK_CONST1:
        case TOK_CONST2:
        case TOK_CONST3:
            qualifiers |= VT_CONSTANT;
            goto redo;
        case TOK_VOLATILE1:
        case TOK_VOLATILE2:
        case TOK_VOLATILE3:
            qualifiers |= VT_VOLATILE;
            goto redo;
        case TOK_RESTRICT1:
        case TOK_RESTRICT2:
        case TOK_RESTRICT3:
            goto redo;
	/* XXX: clarify attribute handling */
	case TOK_ATTRIBUTE1:
	case TOK_ATTRIBUTE2:
	    parse_attribute(ad);
	    break;
        }
        mk_pointer(type);
        type->t |= qualifiers;
	if (ret == type)
	    /* innermost pointed to type is the one for the first derivation */
	    ret = pointed_type(type);
    }

    if (tok == '(') {
	/* This is possibly a parameter type list for abstract declarators
	   ('int ()'), use post_type for testing this.  */
	if (!post_type(type, ad, 0, td)) {
	    /* It's not, so it's a nested declarator, and the post operations
	       apply to the innermost pointed to type (if any).  */
	    /* XXX: this is not correct to modify 'ad' at this point, but
	       the syntax is not clear */
	    parse_attribute(ad);
	    post = type_decl(type, ad, v, td);
	    skip(')');
	} else
	  goto abstract;
    } else if (tok >= TOK_IDENT && (td & TYPE_DIRECT)) {
	/* type identifier */
	*v = tok;
	next();
    } else {
  abstract:
	if (!(td & TYPE_ABSTRACT))
	  expect("identifier");
	*v = 0;
    }
    post_type(post, ad, post != ret ? 0 : storage,
              td & ~(TYPE_DIRECT|TYPE_ABSTRACT));
    parse_attribute(ad);
    type->t |= storage;
    return ret;
}

/* indirection with full error checking and bound check */
ST_FUNC void indir(void)
{
    if ((vtop->type.t & VT_BTYPE) != VT_PTR) {
        if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
            return;
        expect("pointer");
    }
    if (vtop->r & VT_LVAL)
        gv(RC_INT);
    /* warn on dereferencing a pointer that may be NULL (allocator return)
       while the value is still the direct result of the call (same expression) */
    if ((vtop->r & VT_MAYNULL))
        tcc_warning_c(warn_nonnull_deref)(
            "possible NULL pointer dereference (allocator/pointer-returning call); "
            "NULL-check the value before '%s'", "(*/->/[])");
    vtop->type = *pointed_type(&vtop->type);
    /* Arrays and functions are never lvalues */
    if (!(vtop->type.t & (VT_ARRAY | VT_VLA))
        && (vtop->type.t & VT_BTYPE) != VT_FUNC) {
        vtop->r |= VT_LVAL;
        /* if bound checking, the referenced pointer must be checked */
#ifdef CONFIG_TCC_BCHECK
        if (tcc_state->do_bounds_check)
            vtop->r |= VT_MUSTBOUND;
#endif
    }
}

/* pass a parameter to a function and do type checking and casting */
static void gfunc_param_typed(Sym *func, Sym *arg)
{
    int func_type;
    CType type;

    func_type = func->f.func_type;
    if (func_type == FUNC_OLD ||
        (func_type == FUNC_ELLIPSIS && arg == NULL)) {
        /* default casting : only need to convert float to double */
        if ((vtop->type.t & VT_BTYPE) == VT_FLOAT) {
            gen_cast_s(VT_DOUBLE);
        } else if (vtop->type.t & VT_BITFIELD) {
            type.t = vtop->type.t & (VT_BTYPE | VT_UNSIGNED);
	    type.ref = vtop->type.ref;
            gen_cast(&type);
        } else if (vtop->r & VT_MUSTCAST) {
            force_charshort_cast();
        }
    } else if (arg == NULL) {
        tcc_error("too many arguments to function");
    } else {
        type = arg->type;
        type.t &= ~VT_CONSTANT; /* need to do that to avoid false warning */
        gen_assign_cast(&type);
    }
}

/* parse an expression and return its type without any side effect. */
static void expr_type(CType *type, void (*expr_fn)(void))
{
    nocode_wanted++;
    expr_fn();
    *type = vtop->type;
    vpop();
    nocode_wanted--;
}

/* parse an expression of the form '(type)' or '(expr)' and return its
   type */
static void parse_expr_type(CType *type)
{
    int n;
    AttributeDef ad;

    skip('(');
    if (parse_btype(type, &ad, 0)) {
        type_decl(type, &ad, &n, TYPE_ABSTRACT);
    } else {
        expr_type(type, gexpr);
    }
    skip(')');
}

static void parse_type(CType *type)
{
    AttributeDef ad;
    int n;

    if (!parse_btype(type, &ad, 0)) {
        expect("type");
    }
    type_decl(type, &ad, &n, TYPE_ABSTRACT);
}

static void parse_builtin_params(int nc, const char *args)
{
    char c, sep = '(';
    CType type;
    if (nc)
        nocode_wanted++;
    next();
    if (*args == 0)
	skip(sep);
    while ((c = *args++)) {
	skip(sep);
	sep = ',';
        if (c == 't') {
            parse_type(&type);
	    vpush(&type);
	    continue;
        }
        expr_eq();
        type.ref = NULL;
        type.t = 0;
	switch (c) {
	    case 'e':
		continue;
	    case 'V':
                type.t = VT_CONSTANT;
	    case 'v':
                type.t |= VT_VOID;
                mk_pointer (&type);
                break;
	    case 'S':
                type.t = VT_CONSTANT;
	    case 's':
                type.t |= char_type.t;
                mk_pointer (&type);
                break;
	    case 'i':
                type.t = VT_INT;
                break;
	    case 'l':
                type.t = VT_SIZE_T;
                break;
	    default:
                break;
	}
        gen_assign_cast(&type);
    }
    skip(')');
    if (nc)
        nocode_wanted--;
}

static void parse_atomic(int atok)
{
    int size, align, arg, t, save = 0;
    CType *atom, *atom_ptr, ct = {0};
    SValue store;
    char buf[40];
    static const char *const templates[] = {
        /*
         * Each entry consists of callback and function template.
         * The template represents argument types and return type.
         *
         * ? void (return-only)
         * b bool
         * a atomic
         * A read-only atomic
         * p pointer to memory
         * v value
         * l load pointer
         * s save pointer
         * m memory model
         */

        /* keep in order of appearance in tcctok.h: */
        /* __atomic_store */            "alm.?",
        /* __atomic_load */             "Asm.v",
        /* __atomic_exchange */         "alsm.v",
        /* __atomic_compare_exchange */ "aplbmm.b",
        /* __atomic_fetch_add */        "avm.v",
        /* __atomic_fetch_sub */        "avm.v",
        /* __atomic_fetch_or */         "avm.v",
        /* __atomic_fetch_xor */        "avm.v",
        /* __atomic_fetch_and */        "avm.v",
        /* __atomic_fetch_nand */       "avm.v",
        /* __atomic_and_fetch */        "avm.v",
        /* __atomic_sub_fetch */        "avm.v",
        /* __atomic_or_fetch */         "avm.v",
        /* __atomic_xor_fetch */        "avm.v",
        /* __atomic_and_fetch */        "avm.v",
        /* __atomic_nand_fetch */       "avm.v"
    };
    const char *template = templates[(atok - TOK___atomic_store)];

    atom = atom_ptr = NULL;
    size = 0; /* pacify compiler */
    next();
    skip('(');
    for (arg = 0;;) {
        expr_eq();
        switch (template[arg]) {
        case 'a':
        case 'A':
            atom_ptr = &vtop->type;
            if ((atom_ptr->t & VT_BTYPE) != VT_PTR)
                expect("pointer");
            atom = pointed_type(atom_ptr);
            size = type_size(atom, &align);
            if (size > 8
                || (size & (size - 1))
                || (atok > TOK___atomic_compare_exchange
                    && (0 == btype_size(atom->t & VT_BTYPE)
                        || (atom->t & VT_BTYPE) == VT_PTR)))
                expect("integral or integer-sized pointer target type");
            /* GCC does not care either: */
            /* if (!(atom->t & VT_ATOMIC))
                tcc_warning("pointer target declaration is missing '_Atomic'"); */
            break;

        case 'p':
            if ((vtop->type.t & VT_BTYPE) != VT_PTR
             || type_size(pointed_type(&vtop->type), &align) != size)
                tcc_error("pointer target type mismatch in argument %d", arg + 1);
            gen_assign_cast(atom_ptr);
            break;
        case 'v':
            gen_assign_cast(atom);
            break;
        case 'l':
            indir();
            gen_assign_cast(atom);
            break;
        case 's':
            save = 1;
            indir();
            store = *vtop;
            vpop();
            break;
        case 'm':
            gen_assign_cast(&int_type);
            break;
        case 'b':
            ct.t = VT_BOOL;
            gen_assign_cast(&ct);
            break;
        }
        if ('.' == template[++arg])
            break;
        skip(',');
    }
    skip(')');

    ct.t = VT_VOID;
    switch (template[arg + 1]) {
    case 'b':
        ct.t = VT_BOOL;
        break;
    case 'v':
        ct = *atom;
        break;
    }

    sprintf(buf, "%s_%d", get_tok_str(atok, 0), size);
    vpush_helper_func(tok_alloc_const(buf));
    vrott(arg - save + 1);
    gfunc_call(arg - save);

    vpush(&ct);
    PUT_R_RET(vtop, ct.t);
    t = ct.t & VT_BTYPE;
    if (t == VT_BYTE || t == VT_SHORT || t == VT_BOOL) {
#ifdef PROMOTE_RET
        vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
        vtop->type.t = VT_INT;
#endif
    }
    gen_cast(&ct);
    if (save) {
        vpush(&ct);
        *vtop = store;
        vswap();
        vstore();
    }
}

/* ---- SIMD B1 内置函数支持 ---------------------------------------------
 * 方案: 128-bit 向量永远存于 16 字节对齐的内存槽(临时局部变量), 不引入
 * 寄存器分配器/XMM 分配改动. 仅添加 _mm_* 内置函数直接从 vstack 的内存
 * 操作数发射打包 SSE 指令. 向量体现在被建模为 16 字节对齐的 struct
 * (v4f/v2d/v4i), 因此赋值/返回/传参等通用机制照常工作.
 *
 * kind: SIMD_F4(float*4, movaps/addps...), SIMD_D2(double*2, movapd/addpd...,
 * 与 ps 同 opcode), SIMD_I4(int32*4, movdqa/paddd/psubd/pmulld + 标量 idiv).
 * --------------------------------------------------------------------- */

enum { SIMD_F4 = 0, SIMD_D2, SIMD_I4, SIMD_W16, SIMD_B8 };

/* 每个 kind 的参数: vtype 名, mov撤指令(int 打包用 movdqa), add/sub/mul opcode,
   打包乘法是否可用, 元素字节数 */
typedef struct {
    const char *name;      /* <simd.h> 类型名 */
    int is_int;            /* 整型: 需 66 前缀 + 标量除法 */
    int esize;             /* 元素字节: 4/2/1 */
    int add_op, sub_op, mul_op;  /* 打包 opcode (mul 0=无打包用标量) */
    const char *nm3;       /* tok_alloc 长度提示 (3 字母) */
} SimdKindInfo;

static const SimdKindInfo simd_kind[5] = {
    { "v4f",  0, 4, 0x58, 0x5c, 0x59, "v4f" },  /* F4: 无前缀 */
    { "v2d",  0, 8, 0x58, 0x5c, 0x59, "v2d" },  /* D2: 66 前缀, 与 ps 同 opcode */
    { "v4i",  1, 4, 0xfe, 0xfa, 0x40, "v4i" },  /* I4: pmulld(SSE4.1) */
    { "v8h",  1, 2, 0xfd, 0xf9, 0xd5, "v8h" },  /* W16: pmullw(SSE2) */
    { "v16b", 1, 1, 0xfc, 0xf8, 0,    "v16b" }, /* B8: 无打包乘法 -> 标量 */
};

/* 取得用户 <simd.h> 中的向量类型 (16 字节对齐 struct). */
static CType simd_vtype(int kind)
{
    CType t;
    const char *nm = simd_kind[kind].name;
    Sym *s = sym_find(tok_alloc(nm, strlen(nm))->tok);
    if (!s || ((s->type.t & (VT_BTYPE|VT_TYPEDEF)) != VT_STRUCT
               && !(s->type.t & VT_STRUCT))) {
        tcc_error("vector type %s required (include <simd.h>)", nm);
    }
    t = s->type;
    return t;
}

/* 把已填充好的 16 字节对齐槽 (dst, r2) 作为 kind 向量结果压栈. */
static void simd_finish_result(int kind, int dst, int r2)
{
    CType t = simd_vtype(kind);
    vset(&t, VT_LOCAL | VT_LVAL, dst);
    vtop->r2 = r2;              /* 标记槽在用, 防止被打包前复用 */
}

/* 整型集合 (I4/W16/B8): 需要用 66 前缀, 清零用 pxor, 除法走标量兜底. */
static int simd_is_int(int kind)
{
    return simd_kind[kind].is_int;
}

/* _mm_setzero_<t>(): 清 XMM0 后存入槽. F4 xorps / D2 xorpd / I4 pxor. */
static void simd_emit_vzero(int kind)
{
    int dst, r2;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    if (simd_is_int(kind))
        o(0xc0ef0f66);                        /* pxor xmm0,xmm0 = 66 0F EF C0 */
    else if (kind == SIMD_D2)
        o(0xc0570f66);                        /* xorpd xmm0,xmm0 = 66 0F 57 C0 */
    else
        o(0xc0570f);                          /* xorps xmm0,xmm0 = 0F 57 C0 */
    if (simd_is_int(kind))
        gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);    /* movdqa [dst],xmm0 */
    else if (kind == SIMD_D2)
        gen_v128_pi(0x29, VT_LOCAL, NULL, dst);    /* movapd [dst],xmm0 */
    else
        gen_v128(0x29, VT_LOCAL, NULL, dst);       /* movaps [dst],xmm0 */
    simd_finish_result(kind, dst, r2);
}

/* _mm_load_<t>(p): p 为 16 字节对齐数据指针. xmm0<-[p] then [dst]<-xmm0. */
static void simd_emit_load(int kind)
{
    int rp, dst, r2;
    save_reg(TREG_XMM0);
    rp = gv(RC_INT);              /* 指针 => 整数寄存器 */
    dst = get_temp_local_var(16, 16, &r2);
    if (kind == SIMD_F4) {        /* float: movaps (no prefix) */
        gen_v128(0x28, rp | VT_LVAL, NULL, 0);   /* movaps  xmm0,[p]  */
        gen_v128(0x29, VT_LOCAL, NULL, dst);     /* movaps  [dst],xmm0 */
    } else if (kind == SIMD_D2) { /* double: movapd (66 prefix) */
        gen_v128_pi(0x28, rp | VT_LVAL, NULL, 0);   /* movapd xmm0,[p]  */
        gen_v128_pi(0x29, VT_LOCAL, NULL, dst);     /* movapd [dst],xmm0 */
    } else {                      /* int32: movdqa (66 prefix) */
        gen_v128_pi(0x6f, rp | VT_LVAL, NULL, 0);   /* movdqa xmm0,[p]  */
        gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);     /* movdqa [dst],xmm0 */
    }
    vtop--;                       /* 弹出指针实参 */
    simd_finish_result(kind, dst, r2);
}

/* _mm_store_<t>(p,v): 把槽 v 写回 16 字节对齐数据指针 p. */
static void simd_emit_store(int kind)
{
    int voff, rp;
    save_reg(TREG_XMM0);
    voff = (int)vtop->c.i;        /* vtop[0]=v 的槽偏移 */
    vswap();
    rp = gv(RC_INT);              /* vtop[-1]=p -> 地址寄存器 */
    if (kind == SIMD_F4) {        /* float: movaps (no prefix) */
        gen_v128(0x28, VT_LOCAL, NULL, voff);   /* movaps xmm0,[v]    */
        gen_v128(0x29, rp | VT_LVAL, NULL, 0);  /* movaps [p],xmm0    */
    } else if (kind == SIMD_D2) { /* double: movapd (66 prefix) */
        gen_v128_pi(0x28, VT_LOCAL, NULL, voff);   /* movapd xmm0,[v] */
        gen_v128_pi(0x29, rp | VT_LVAL, NULL, 0);  /* movapd [p],xmm0 */
    } else {                      /* int32: movdqa (66 prefix) */
        gen_v128_pi(0x6f, VT_LOCAL, NULL, voff);   /* movdqa xmm0,[v] */
        gen_v128_pi(0x7f, rp | VT_LVAL, NULL, 0);  /* movdqa [p],xmm0 */
    }
    vtop -= 2;                    /* 丢弃 p,v */
    {
        CType vt;                 /* void 语句: 留 void 值供收尾弹栈 */
        vt.t = VT_VOID;
        vpush(&vt);
    }
}

/* _mm_<op>_<t>(a,b): vtop[-1]=a, vtop[0]=b (均为内存槽). op 为 op 下标:
   0 add / 1 sub / 2 mul / 3 div. us=1 表示无符号除法(仅整型除法用 idiv vs div).
   直接把两个槽址作为 [rbp+off] 内存操作数发射. F4/D2 走打包
   addps~divps/addpd~divpd(同 opcode); 整型加/减/乘(若可用)走打包,
   整型除法(及无打包乘法)走标量 GPR 兜底. */
static void simd_emit_binop(int kind, int op, int us)
{
    static const int pd_op[4] = {0x58, 0x5c, 0x59, 0x5e}; /* addps~divps/pd */
    int ao, bo, dst, r2;
    const SimdKindInfo *ki = &simd_kind[kind];
    ao = (int)vtop[-1].c.i;
    bo = (int)vtop[0].c.i;
    save_reg(TREG_XMM0);        /* 若 xmm0 正持有活跃标量浮点, 先落内存 */

    if (simd_is_int(kind)) {
        int esize = ki->esize;
        int n = 16 / esize;     /* 槽 = 16 字节 / 元素字节 */
        if (op == 3) {          /* 除法: 无打包 -> 标量 (idiv 或 div) */
            save_reg(TREG_RAX); save_reg(TREG_RDX); save_reg(TREG_RCX);
            dst = get_temp_local_var(16, 16, &r2);
            gen_vec_div(esize, n, us, ao, bo, dst);
            vtop -= 2;
            simd_finish_result(kind, dst, r2);
            return;
        }
        dst = get_temp_local_var(16, 16, &r2);
        if (op == 2 && ki->mul_op == 0) {   /* 8bit: 无打包乘法 -> 标量 */
            gen_vec_mul8(n, ao, bo, dst);
        } else {
            int mop = op == 0 ? ki->add_op : op == 1 ? ki->sub_op : ki->mul_op;
            gen_v128_pi(0x6f, VT_LOCAL, NULL, ao);      /* movdqa xmm0,[a] */
            if (kind == SIMD_I4 && op == 2)
                gen_v128_sse41(mop, VT_LOCAL, NULL, bo);   /* pmulld (SSE4.1) */
            else
                gen_v128_pi(mop, VT_LOCAL, NULL, bo);      /* paddb/psubb/... */
            gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);      /* movdqa [dst],xmm0 */
        }
        vtop -= 2;
        simd_finish_result(kind, dst, r2);
        return;
    }
    dst = get_temp_local_var(16, 16, &r2);
    if (kind == SIMD_F4) {            /* float: movaps/addps... no prefix */
        gen_v128(0x28, VT_LOCAL, NULL, ao);            /* movaps xmm0,[a] */
        gen_v128(pd_op[op], VT_LOCAL, NULL, bo);       /* opxps  xmm0,[b] */
        gen_v128(0x29, VT_LOCAL, NULL, dst);           /* movaps [dst],xmm0 */
    } else {                          /* double: movapd/addpd... 66 prefix */
        gen_v128_pi(0x28, VT_LOCAL, NULL, ao);         /* movapd xmm0,[a] */
        gen_v128_pi(pd_op[op], VT_LOCAL, NULL, bo);    /* opxpd  xmm0,[b] */
        gen_v128_pi(0x29, VT_LOCAL, NULL, dst);        /* movapd [dst],xmm0 */
    }
    vtop -= 2;                  /* 弹出已消费的 a,b */
    simd_finish_result(kind, dst, r2);
}

/* ---- SIMD 常用扩展 (位运算/minmax/sqrt/比较/移位/转换) --------------- */

/* 位运算 opcode: and/or/xor/andnot. 对 ps/pd 同值; int(si128) 用 .1 覆盖. */
static const int simd_bitop[4]   = {0x54, 0x56, 0x57, 0x55};
/* 整型(si128)位运算 opcode: pand/por/pxor/pandn.
   ps/pd 的 0x54/56/57/55 加 66 前缀会解码成 andpd/orpd/xorpd/andnpd,
   而非整型的 pand/por/pxor/pandn (0xDB/EB/EF/DF). */
static const int simd_bitop_i[4] = {0xDB, 0xEB, 0xEF, 0xDF};
/* float min/max opcode (minps maxps / minpd maxpd 同值). */
static const int simd_minmax_f[2] = {0x5d, 0x5f};
/* int32 min/max opcode (SSE4.1): min_s/max_s/min_u/max_u. */
static const int simd_minmax_i[4] = {0x39, 0x3d, 0x3b, 0x3f};
/* float 比较谓词 imm (cmpps/cmppd): eq neq lt le gt ge. gt/ge 交换操作数. */
static const int simd_cmpf_imm[6]  = {0x0, 0x4, 0x1, 0x2, 0x1, 0x2};
static const int simd_cmpf_swap[6] = {0,   0,   0,   0,   1,   1};

/* 通用二目打包指令 (位运算/min/max): vtop[-1]=a, vtop[0]=b -> xmm0=a OP b -> 新槽.
   prefix: 0=无(F4 andps...), 1=66(D2/I4), 2=SSE4.1 三字节(I4). */
static void simd_emit_membin(int kind, int opcode, int prefix)
{
    int ao, bo, dst, r2;
    ao = (int)vtop[-1].c.i;
    bo = (int)vtop[0].c.i;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    if (prefix == 2) {            /* SSE4.1: pminsd/pmaxsd/... */
        gen_v128_pi(0x6f, VT_LOCAL, NULL, ao);   /* movdqa xmm0,[a] */
        gen_v128_sse41(opcode, VT_LOCAL, NULL, bo);
        gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);  /* movdqa [dst],xmm0 */
    } else if (kind == SIMD_F4) { /* float32: 无前缀 */
        gen_v128(0x28, VT_LOCAL, NULL, ao);      /* movaps xmm0,[a] */
        gen_v128(opcode, VT_LOCAL, NULL, bo);
        gen_v128(0x29, VT_LOCAL, NULL, dst);     /* movaps [dst],xmm0 */
    } else if (simd_is_int(kind)) { /* 整型: 66 + movdqa */
        gen_v128_pi(0x6f, VT_LOCAL, NULL, ao);
        gen_v128_pi(opcode, VT_LOCAL, NULL, bo);
        gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);
    } else {                      /* double: 66 + movapd */
        gen_v128_pi(0x28, VT_LOCAL, NULL, ao);
        gen_v128_pi(opcode, VT_LOCAL, NULL, bo);
        gen_v128_pi(0x29, VT_LOCAL, NULL, dst);
    }
    vtop -= 2;
    simd_finish_result(kind, dst, r2);
}

/* 一元 sqrt: vtop[0]=a. sqrtps/sqrtpd. */
static void simd_emit_sqrt(int kind)
{
    int ao, dst, r2;
    ao = (int)vtop[0].c.i;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    if (kind == SIMD_F4) {
        gen_v128(0x51, VT_LOCAL, NULL, ao);      /* sqrtps xmm0,[a] */
        gen_v128(0x29, VT_LOCAL, NULL, dst);
    } else {
        gen_v128_pi(0x51, VT_LOCAL, NULL, ao);   /* sqrtpd */
        gen_v128_pi(0x29, VT_LOCAL, NULL, dst);
    }
    vtop--;
    simd_finish_result(kind, dst, r2);
}

/* float 比较 -> 掩码槽 (全 0/全 F). imm=CMPPS pred, swap=1 对调 a/b (gt/ge). */
static void simd_emit_cmpf(int kind, int imm, int swap)
{
    int ao, bo, src, cmp, dst, r2;
    ao  = (int)vtop[-1].c.i;
    bo  = (int)vtop[0].c.i;
    src = swap ? bo : ao;
    cmp = swap ? ao : bo;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    if (kind == SIMD_F4) {
        gen_v128(0x28, VT_LOCAL, NULL, src);        /* movaps xmm0,[src] */
        gen_v128_cmp(0, imm, VT_LOCAL, NULL, cmp);  /* cmpps  xmm0,[cmp],imm */
        gen_v128(0x29, VT_LOCAL, NULL, dst);        /* movaps [dst],xmm0 */
    } else {
        gen_v128_pi(0x28, VT_LOCAL, NULL, src);     /* movapd */
        gen_v128_cmp(1, imm, VT_LOCAL, NULL, cmp);  /* cmppd  xmm0,[cmp],imm */
        gen_v128_pi(0x29, VT_LOCAL, NULL, dst);     /* movapd [dst],xmm0 */
    }
    vtop -= 2;
    simd_finish_result(kind, dst, r2);
}

/* 整型比较 -> v4i 掩码. pcmpeqd(0x74) a==b / pcmpgtd(0x66) a>b (a<b 交换). */
static void simd_emit_cmpi(int opcode, int swap)
{
    int ao, bo, src, cmp, dst, r2;
    ao  = (int)vtop[-1].c.i;
    bo  = (int)vtop[0].c.i;
    src = swap ? bo : ao;
    cmp = swap ? ao : bo;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    gen_v128_pi(0x6f, VT_LOCAL, NULL, src);   /* movdqa xmm0,[src] */
    gen_v128_pi(opcode, VT_LOCAL, NULL, cmp);
    gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);   /* movdqa [dst],xmm0 */
    vtop -= 2;
    simd_finish_result(SIMD_I4, dst, r2);
}

/* int32 移位 imm: vtop[-1]=a, vtop[0]=count(编译期常量). */
static void simd_emit_shift(int opcode)
{
    int ao, count, dst, r2;
    ao    = (int)vtop[-1].c.i;
    count = (int)vtop[0].c.i;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    gen_v128_pi(0x6f, VT_LOCAL, NULL, ao);   /* movdqa xmm0,[a] */
    gen_v128_shift(opcode, count);
    gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);
    vtop -= 2;
    simd_finish_result(SIMD_I4, dst, r2);
}

/* 转换 cvtdq2ps: v4i -> v4f. */
static void simd_emit_i4f4(void)
{
    int ao, dst, r2;
    ao = (int)vtop[0].c.i;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    gen_v128(0x5b, VT_LOCAL, NULL, ao);   /* cvtdq2ps xmm0,[a] = 0F 5B */
    gen_v128(0x29, VT_LOCAL, NULL, dst);  /* movaps [dst],xmm0 */
    vtop--;
    simd_finish_result(SIMD_F4, dst, r2);
}

/* 转换 v4f -> v4i: trunc=1 cvttps2dq(F2 0F 5B 截断), else cvtps2dq(66 0F 5B 舍入). */
static void simd_emit_f4i4(int trunc)
{
    int ao, dst, r2;
    ao = (int)vtop[0].c.i;
    save_reg(TREG_XMM0);
    dst = get_temp_local_var(16, 16, &r2);
    if (trunc)
        gen_v128_f3(0x5b, VT_LOCAL, NULL, ao);  /* cvttps2dq: F3 0F 5B (截断) */
    else
        gen_v128_pi(0x5b, VT_LOCAL, NULL, ao);  /* cvtps2dq:  66 0F 5B (舍入) */
    gen_v128_pi(0x7f, VT_LOCAL, NULL, dst);     /* movdqa [dst],xmm0 */
    vtop--;
    simd_finish_result(SIMD_I4, dst, r2);
}

ST_FUNC void unary(void)
{
    int n, t, align, size, r;
    CType type;
    Sym *s;
    AttributeDef ad;

    /* generate line number info */
    if (debug_modes)
        tcc_debug_line(tcc_state), tcc_tcov_check_line (tcc_state, 1);

    type.ref = NULL;
    /* XXX: GCC 2.95.3 does not generate a table although it should be
       better here */
 tok_next:
    switch(tok) {
    case TOK_EXTENSION:
        next();
        goto tok_next;
    case TOK_LCHAR:
#ifdef TCC_TARGET_PE
        t = VT_SHORT|VT_UNSIGNED;
        goto push_tokc;
#endif
    case TOK_CINT:
    case TOK_CCHAR: 
	t = VT_INT;
 push_tokc:
	type.t = t;
	vsetc(&type, VT_CONST, &tokc);
        next();
        break;
    case TOK_CUINT:
        t = VT_INT | VT_UNSIGNED;
        goto push_tokc;
    case TOK_CLLONG:
        t = VT_LLONG;
	goto push_tokc;
    case TOK_CULLONG:
        t = VT_LLONG | VT_UNSIGNED;
	goto push_tokc;
    case TOK_CFLOAT:
        t = VT_FLOAT;
	goto push_tokc;
    case TOK_CDOUBLE:
        t = VT_DOUBLE;
	goto push_tokc;
    case TOK_CLDOUBLE:
#ifdef TCC_USING_DOUBLE_FOR_LDOUBLE
        t = VT_DOUBLE | VT_LONG;
        tokc.d = tokc.ld;
#else
        t = VT_LDOUBLE;
#endif
	goto push_tokc;
    case TOK_CLONG:
        t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG;
	goto push_tokc;
    case TOK_CULONG:
        t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG | VT_UNSIGNED;
	goto push_tokc;
    case TOK___FUNCTION__:
        if (!gnu_ext)
            goto tok_identifier;
        /* fall thru */
    case TOK___FUNC__:
        tok = TOK_STR;
        cstr_reset(&tokcstr);
        cstr_cat(&tokcstr, funcname, 0);
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        goto case_TOK_STR;
    case TOK_LSTR:
        /* tcc_posix: wchar_t 统一 4 字节 (musl), PE 不再用 unsigned short */
        t = VT_INT;
        goto str_init;
    case TOK_STR:
    case_TOK_STR:
        /* string parsing */
        t = char_type.t;
    str_init:
        if (tcc_state->warn_write_strings & WARN_ON)
            t |= VT_CONSTANT;
        type.t = t;
        mk_pointer(&type);
        type.t |= VT_ARRAY;
        memset(&ad, 0, sizeof(AttributeDef));
        ad.section = rodata_section;
        decl_initializer_alloc(&type, &ad, VT_CONST, 2, 0, 0);
        break;
    case TOK_SOTYPE:
    case '(':
        t = tok;
        next();
        /* cast ? */
        if (parse_btype(&type, &ad, 0)) {
            type_decl(&type, &ad, &n, TYPE_ABSTRACT);
            skip(')');
            /* check ISOC99 compound literal */
            if (tok == '{') {
                    /* data is allocated locally by default */
                if (global_expr)
                    r = VT_CONST;
                else
                    r = VT_LOCAL;
                /* all except arrays are lvalues */
                if (!(type.t & VT_ARRAY))
                    r |= VT_LVAL;
                memset(&ad, 0, sizeof(AttributeDef));
                decl_initializer_alloc(&type, &ad, r, 1, 0, 0);
            } else if (t == TOK_SOTYPE) { /* from sizeof/alignof (...) */
                vpush(&type);
                return;
            } else {
                unary();
                gen_cast(&type);
            }
        } else if (tok == '{') {
	    int saved_nocode_wanted = nocode_wanted;
            if (CONST_WANTED && !NOEVAL_WANTED)
                expect("constant");
            if (0 == local_scope)
                tcc_error("statement expression outside of function");
            /* save all registers */
            save_regs(0);
            /* statement expression : we do not accept break/continue
               inside as GCC does.  We do retain the nocode_wanted state,
	       as statement expressions can't ever be entered from the
	       outside, so any reactivation of code emission (from labels
	       or loop heads) can be disabled again after the end of it. */
            /* default return value is (void) */
            vpushi(0), vtop->type.t = VT_VOID;
            block(STMT_EXPR);
            /* If the statement expr can be entered, then we retain the current
               nocode_wanted state (from e.g. a 'return 0;' in the stmt-expr).
               If it can't be entered then the state is that from before the
               statement expression.  */
            if (saved_nocode_wanted)
              nocode_wanted = saved_nocode_wanted;
            skip(')');
        } else {
            gexpr();
            skip(')');
        }
        break;
    case '*':
        next();
        unary();
        indir();
        break;
    case '&':
        next();
        unary();
        /* functions names must be treated as function pointers,
           except for unary '&' and sizeof. Since we consider that
           functions are not lvalues, we only have to handle it
           there and in function calls. */
        /* arrays can also be used although they are not lvalues */
        if ((vtop->type.t & VT_BTYPE) != VT_FUNC &&
            !(vtop->type.t & (VT_ARRAY | VT_VLA)))
            test_lvalue();
        if (vtop->sym)
          vtop->sym->a.addrtaken = 1;
        mk_pointer(&vtop->type);
        gaddrof();
        break;
    case '!':
        next();
        unary();
        gen_test_zero(TOK_EQ);
        break;
    case '~':
        next();
        unary();
        vpushi(-1);
        gen_op('^');
        break;
    case '+':
        next();
        unary();
        if ((vtop->type.t & VT_BTYPE) == VT_PTR)
            tcc_error("pointer not accepted for unary plus");
        /* In order to force cast, we add zero, except for floating point
	   where we really need an noop (otherwise -0.0 will be transformed
	   into +0.0).  */
	if (!is_float(vtop->type.t)) {
	    vpushi(0);
	    gen_op('+');
	}
        break;
    case TOK_SIZEOF:
    case TOK_ALIGNOF1:
    case TOK_ALIGNOF2:
    case TOK_ALIGNOF3:
        t = tok;
        next();
        if (tok == '(')
            tok = TOK_SOTYPE;
        expr_type(&type, unary);
        if (t == TOK_SIZEOF) {
            vpush_type_size(&type, &align);
            gen_cast_s(VT_SIZE_T);
        } else {
            type_size(&type, &align);
            s = NULL;
            if (vtop[1].r & VT_SYM)
                s = vtop[1].sym; /* hack: accessing previous vtop */
            if (s && s->a.aligned)
                align = 1 << (s->a.aligned - 1);
            vpushs(align);
        }
        break;

    case TOK_builtin_expect:
	/* __builtin_expect is a no-op for now */
	parse_builtin_params(0, "ee");
	vpop();
        break;
    case TOK_builtin_types_compatible_p:
	parse_builtin_params(0, "tt");
	vtop[-1].type.t &= ~(VT_CONSTANT | VT_VOLATILE);
	vtop[0].type.t &= ~(VT_CONSTANT | VT_VOLATILE);
	n = is_compatible_types(&vtop[-1].type, &vtop[0].type);
	vtop -= 2;
	vpushi(n);
        break;
    case TOK_builtin_choose_expr:
	{
	    int64_t c;
	    next();
	    skip('(');
	    c = expr_const64();
	    skip(',');
	    if (!c) {
		nocode_wanted++;
	    }
	    expr_eq();
	    if (!c) {
		vpop();
		nocode_wanted--;
	    }
	    skip(',');
	    if (c) {
		nocode_wanted++;
	    }
	    expr_eq();
	    if (c) {
		vpop();
		nocode_wanted--;
	    }
	    skip(')');
	}
        break;
    case TOK_builtin_constant_p:
	parse_builtin_params(1, "e");
	n = 1;
	if ((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST
	    || ((vtop->r & VT_SYM) && vtop->sym->a.addrtaken)
	    )
	    n = 0;
	vtop--;
	vpushi(n);
        break;
    case TOK_builtin_unreachable:
	parse_builtin_params(0, ""); /* just skip '()' */
        type.t = VT_VOID;
        vpush(&type);
        CODE_OFF();
        break;
    /* SIMD B1: 内建函数发射打包 SSE 指令 (值存内存槽, 向量类型 v4f/v2d/v4i/v8h/v16b) */
    case TOK_mm_load_ps:   parse_builtin_params(0, "e"); simd_emit_load(SIMD_F4); break;
    case TOK_mm_load_pd:   parse_builtin_params(0, "e"); simd_emit_load(SIMD_D2); break;
    case TOK_mm_load_epi32:parse_builtin_params(0, "e"); simd_emit_load(SIMD_I4); break;
    case TOK_mm_load_epi16:parse_builtin_params(0, "e"); simd_emit_load(SIMD_W16); break;
    case TOK_mm_load_epi8: parse_builtin_params(0, "e"); simd_emit_load(SIMD_B8); break;
    case TOK_mm_store_ps:  parse_builtin_params(0, "ee"); simd_emit_store(SIMD_F4); break;
    case TOK_mm_store_pd:  parse_builtin_params(0, "ee"); simd_emit_store(SIMD_D2); break;
    case TOK_mm_store_epi32:parse_builtin_params(0,"ee"); simd_emit_store(SIMD_I4); break;
    case TOK_mm_store_epi16:parse_builtin_params(0,"ee"); simd_emit_store(SIMD_W16); break;
    case TOK_mm_store_epi8: parse_builtin_params(0,"ee"); simd_emit_store(SIMD_B8); break;
    case TOK_mm_add_ps:  case TOK_mm_sub_ps:  case TOK_mm_mul_ps:  case TOK_mm_div_ps:
        { int simd_op = tok - TOK_mm_add_ps;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_F4, simd_op, 0); }
        break;
    case TOK_mm_add_pd:  case TOK_mm_sub_pd:  case TOK_mm_mul_pd:  case TOK_mm_div_pd:
        { int simd_op = tok - TOK_mm_add_pd;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_D2, simd_op, 0); }
        break;
    case TOK_mm_add_epi32: case TOK_mm_sub_epi32: case TOK_mm_mul_epi32: case TOK_mm_div_epi32:
        { int simd_op = tok - TOK_mm_add_epi32;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_I4, simd_op, 0); }
        break;
    case TOK_mm_add_epi16: case TOK_mm_sub_epi16: case TOK_mm_mul_epi16: case TOK_mm_div_epi16:
        { int simd_op = tok - TOK_mm_add_epi16;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_W16, simd_op, 0); }
        break;
    case TOK_mm_add_epi8:  case TOK_mm_sub_epi8:  case TOK_mm_mul_epi8:  case TOK_mm_div_epi8:
        { int simd_op = tok - TOK_mm_add_epi8;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_B8, simd_op, 0); }
        break;
    case TOK_mm_div_epu32: parse_builtin_params(0, "ee"); simd_emit_binop(SIMD_I4, 3, 1); break;
    case TOK_mm_div_epu16: parse_builtin_params(0, "ee"); simd_emit_binop(SIMD_W16, 3, 1); break;
    case TOK_mm_div_epu8:  parse_builtin_params(0, "ee"); simd_emit_binop(SIMD_B8, 3, 1); break;
    case TOK_mm_setzero_ps:   parse_builtin_params(0, ""); simd_emit_vzero(SIMD_F4); break;
    case TOK_mm_setzero_pd:   parse_builtin_params(0, ""); simd_emit_vzero(SIMD_D2); break;
    case TOK_mm_setzero_epi32:parse_builtin_params(0, ""); simd_emit_vzero(SIMD_I4); break;
    case TOK_mm_setzero_epi16:parse_builtin_params(0, ""); simd_emit_vzero(SIMD_W16); break;
    case TOK_mm_setzero_epi8: parse_builtin_params(0, ""); simd_emit_vzero(SIMD_B8); break;
    /* ---- SIMD 常用扩展: 位运算 ---- */
    case TOK_mm_and_ps: case TOK_mm_or_ps: case TOK_mm_xor_ps: case TOK_mm_andnot_ps:
        { int simd_op = tok - TOK_mm_and_ps;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_F4, simd_bitop[simd_op], 0); }
        break;
    case TOK_mm_and_pd: case TOK_mm_or_pd: case TOK_mm_xor_pd: case TOK_mm_andnot_pd:
        { int simd_op = tok - TOK_mm_and_pd;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_D2, simd_bitop[simd_op], 1); }
        break;
    case TOK_mm_and_si128: case TOK_mm_or_si128: case TOK_mm_xor_si128: case TOK_mm_andnot_si128:
        { int simd_op = tok - TOK_mm_and_si128;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_I4, simd_bitop_i[simd_op], 1); }
        break;
    /* ---- min/max / sqrt ---- */
    case TOK_mm_min_ps: case TOK_mm_max_ps:
        { int simd_op = tok - TOK_mm_min_ps;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_F4, simd_minmax_f[simd_op], 0); }
        break;
    case TOK_mm_min_pd: case TOK_mm_max_pd:
        { int simd_op = tok - TOK_mm_min_pd;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_D2, simd_minmax_f[simd_op], 1); }
        break;
    case TOK_mm_sqrt_ps: parse_builtin_params(0, "e"); simd_emit_sqrt(SIMD_F4); break;
    case TOK_mm_sqrt_pd: parse_builtin_params(0, "e"); simd_emit_sqrt(SIMD_D2); break;
    /* ---- 比较 (float) ---- */
    case TOK_mm_cmpeq_ps: case TOK_mm_cmpneq_ps: case TOK_mm_cmplt_ps: case TOK_mm_cmple_ps:
    case TOK_mm_cmpgt_ps: case TOK_mm_cmpge_ps:
        { int simd_cmp = tok - TOK_mm_cmpeq_ps;
          parse_builtin_params(0, "ee");
          simd_emit_cmpf(SIMD_F4, simd_cmpf_imm[simd_cmp], simd_cmpf_swap[simd_cmp]); }
        break;
    case TOK_mm_cmpeq_pd: case TOK_mm_cmpneq_pd: case TOK_mm_cmplt_pd: case TOK_mm_cmple_pd:
    case TOK_mm_cmpgt_pd: case TOK_mm_cmpge_pd:
        { int simd_cmp = tok - TOK_mm_cmpeq_pd;
          parse_builtin_params(0, "ee");
          simd_emit_cmpf(SIMD_D2, simd_cmpf_imm[simd_cmp], simd_cmpf_swap[simd_cmp]); }
        break;
    /* ---- 比较 / min/max (int32) ---- */
    case TOK_mm_cmpeq_epi32: parse_builtin_params(0, "ee"); simd_emit_cmpi(0x76, 0); break;
    case TOK_mm_cmpgt_epi32: parse_builtin_params(0, "ee"); simd_emit_cmpi(0x66, 0); break;
    case TOK_mm_cmplt_epi32: parse_builtin_params(0, "ee"); simd_emit_cmpi(0x66, 1); break;
    case TOK_mm_min_epi32: case TOK_mm_max_epi32: case TOK_mm_min_epu32: case TOK_mm_max_epu32:
        { int simd_op = tok - TOK_mm_min_epi32;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_I4, simd_minmax_i[simd_op], 2); }
        break;
    /* ---- 移位 (int32, imm) ---- */
    case TOK_mm_slli_epi32: parse_builtin_params(0, "ei"); simd_emit_shift(0xf0); break;
    case TOK_mm_srli_epi32: parse_builtin_params(0, "ei"); simd_emit_shift(0xd0); break;
    case TOK_mm_srai_epi32: parse_builtin_params(0, "ei"); simd_emit_shift(0xe0); break;
    /* ---- 类型转换 int32<->float32 ---- */
    case TOK_mm_cvtepi32_ps: parse_builtin_params(0, "e"); simd_emit_i4f4(); break;
    case TOK_mm_cvtps_epi32: parse_builtin_params(0, "e"); simd_emit_f4i4(0); break;
    case TOK_mm_cvttps_epi32: parse_builtin_params(0, "e"); simd_emit_f4i4(1); break;
    case TOK_builtin_frame_address:
    case TOK_builtin_return_address:
        {
            int tok1 = tok;
            int level;
            next();
            skip('(');
            level = expr_const();
            if (level < 0)
                tcc_error("%s only takes positive integers", get_tok_str(tok1, 0));
            skip(')');
            type.t = VT_VOID;
            mk_pointer(&type);
            vset(&type, VT_LOCAL, 0);       /* local frame */
            while (level--) {
#ifdef TCC_TARGET_RISCV64
                vpushi(2*PTR_SIZE);
                gen_op('-');
#endif
                mk_pointer(&vtop->type);
                indir();                    /* -> parent frame */
            }
            if (tok1 == TOK_builtin_return_address) {
                // assume return address is just above frame pointer on stack
#ifdef TCC_TARGET_ARM
                vpushi(2*PTR_SIZE);
                gen_op('+');
#elif defined TCC_TARGET_RISCV64
                vpushi(PTR_SIZE);
                gen_op('-');
#else
                vpushi(PTR_SIZE);
                gen_op('+');
#endif
                mk_pointer(&vtop->type);
                indir();
            }
        }
        break;
#ifdef TCC_TARGET_RISCV64
    case TOK_builtin_va_start:
        parse_builtin_params(0, "ee");
        r = vtop->r & VT_VALMASK;
        if (r == VT_LLOCAL)
            r = VT_LOCAL;
        if (r != VT_LOCAL)
            tcc_error("__builtin_va_start expects a local variable");
        gen_va_start();
	vstore();
        break;
#endif
#ifdef TCC_TARGET_X86_64
#ifdef TCC_TARGET_PE
    case TOK_builtin_va_start:
	parse_builtin_params(0, "ee");
        r = vtop->r & VT_VALMASK;
        if (r == VT_LLOCAL)
            r = VT_LOCAL;
        if (r != VT_LOCAL)
            tcc_error("__builtin_va_start expects a local variable");
        vtop->r = r;
	vtop->type = char_pointer_type;
	vtop->c.i += 8;
	vstore();
        break;
#else
    case TOK_builtin_va_arg_types:
	parse_builtin_params(0, "t");
	vpushi(classify_x86_64_va_arg(&vtop->type));
	vswap();
	vpop();
	break;
#endif
#endif

#ifdef TCC_TARGET_ARM64
    case TOK_builtin_va_start: {
	parse_builtin_params(0, "ee");
        //xx check types
        gen_va_start();
        vpushi(0);
        vtop->type.t = VT_VOID;
        break;
    }
    case TOK_builtin_va_arg: {
	parse_builtin_params(0, "et");
	type = vtop->type;
	vpop();
        //xx check types
        gen_va_arg(&type);
        vtop->type = type;
        break;
    }
#endif
#ifdef TCC_TARGET_ARM64
    case TOK___arm64_clear_cache: {
	parse_builtin_params(0, "ee");
        gen_clear_cache();
        vpushi(0);
        vtop->type.t = VT_VOID;
        break;
    }
#endif
#ifdef TCC_TARGET_RISCV64
    case TOK___riscv64_clear_cache: {
	parse_builtin_params(0, "ee");
	vpop();
	vpop();
        gen_clear_cache();
        vpushi(0);
        vtop->type.t = VT_VOID;
        break;
    }
#endif

    /* atomic operations */
    case TOK___atomic_store:
    case TOK___atomic_load:
    case TOK___atomic_exchange:
    case TOK___atomic_compare_exchange:
    case TOK___atomic_fetch_add:
    case TOK___atomic_fetch_sub:
    case TOK___atomic_fetch_or:
    case TOK___atomic_fetch_xor:
    case TOK___atomic_fetch_and:
    case TOK___atomic_fetch_nand:
    case TOK___atomic_add_fetch:
    case TOK___atomic_sub_fetch:
    case TOK___atomic_or_fetch:
    case TOK___atomic_xor_fetch:
    case TOK___atomic_and_fetch:
    case TOK___atomic_nand_fetch:
        parse_atomic(tok);
        break;

    /* pre operations */
    case TOK_INC:
    case TOK_DEC:
        t = tok;
        next();
        unary();
        inc(0, t);
        break;
    case '-':
        next();
        unary();
	if (is_float(vtop->type.t)) {
            gen_opif(TOK_NEG);
	} else {
            vpushi(0);
            vswap();
            gen_op('-');
        }
        break;
    case TOK_LAND:
        if (!gnu_ext)
            goto tok_identifier;
        next();
        /* allow to take the address of a label */
        if (tok < TOK_UIDENT)
            expect("label identifier");
        s = label_find(tok);
        if (!s) {
            s = label_push(&global_label_stack, tok, LABEL_FORWARD);
        } else {
            if (s->r == LABEL_DECLARED)
                s->r = LABEL_FORWARD;
        }
        if ((s->type.t & VT_BTYPE) != VT_PTR) {
            s->type.t = VT_VOID;
            mk_pointer(&s->type);
            s->type.t |= VT_STATIC;
        }
        vpushsym(&s->type, s);
        next();
        break;

    case TOK_GENERIC:
    {
	CType controlling_type;
	int has_default = 0;
	int has_match = 0;
	int learn = 0;
	TokenString *str = NULL;
	int saved_nocode_wanted = nocode_wanted;
        nocode_wanted &= ~CONST_WANTED_MASK;

        next();
	skip('(');
	expr_type(&controlling_type, expr_eq);
	convert_parameter_type (&controlling_type);

        nocode_wanted = saved_nocode_wanted;

        for (;;) {
	    learn = 0;
	    skip(',');
	    if (tok == TOK_DEFAULT) {
		if (has_default)
		    tcc_error("too many 'default'");
		has_default = 1;
		if (!has_match)
		    learn = 1;
		next();
	    } else {
		int v;
		parse_btype(&type, &ad, 0);
		type_decl(&type, &ad, &v, TYPE_ABSTRACT);
		if (compare_types(&controlling_type, &type, 0)) {
		    if (has_match) {
		      tcc_error("type match twice");
		    }
		    has_match = 1;
		    learn = 1;
		}
	    }
	    skip(':');
	    if (learn) {
		if (str)
		    tok_str_free(str);
		skip_or_save_block(&str);
	    } else {
		skip_or_save_block(NULL);
	    }
	    if (tok == ')')
		break;
	}
	if (!str) {
	    char buf[60];
	    type_to_str(buf, sizeof buf, &controlling_type, NULL);
	    tcc_error("type '%s' does not match any association", buf);
	}
	begin_macro(str, 1);
	next();
	expr_eq();
	if (tok != TOK_EOF)
	    expect(",");
	end_macro();
        next();
	break;
    }
    // special qnan , snan and infinity values
    case TOK___NAN__:
        n = 0x7fc00000;
special_math_val:
	vpushi(n);
	vtop->type.t = VT_FLOAT;
        next();
        break;
    case TOK___SNAN__:
	n = 0x7f800001;
	goto special_math_val;
    case TOK___INF__:
	n = 0x7f800000;
	goto special_math_val;

    default:
    tok_identifier:
        if (tok < TOK_UIDENT)
            tcc_error("expression expected before '%s'", get_tok_str(tok, &tokc));
        t = tok;
        next();
        /* model function 泛型: Name(类型实参)(调用参数) — 优先于普通符号 */
        if (tok == '(') {
            ModelDef *md = model_find(t);
            if (md && md->kind == VT_FUNC &&
                model_function_call(t, md)) {
                /* 实例化完成: vtop = 内部函数符号, tok = 调用参数 '(' */
                break;
            }
        }
        s = sym_find(t);
        if (!s || IS_ASM_SYM(s)) {
            const char *name = get_tok_str(t, NULL);
            if (tok != '(')
                tcc_error("'%s' undeclared", name);
            /* for simple function calls, we tolerate undeclared
               external reference to int() function */
            if (!func_old)
            tcc_warning_c(warn_implicit_function_declaration)(
                "implicit declaration of function '%s'", name);
            s = external_global_sym(t, &func_old_type);
        }

        if (s->type.t & VT_TLS) {
            /* tcc_posix `__thread` -> emutls: the lvalue is
               *(T*)__emutls_get_address(&__emutls_v_<name>).  The descriptor
               was emitted at declaration (tccgen decl, VT_TLS branch). */
            char nbuf[96];
            Sym *ds, *fn;
            snprintf(nbuf, sizeof nbuf, "__emutls_v_%s",
                     get_tok_str(s->v, NULL));
            ds = sym_find(tok_alloc_const(nbuf));
            if (!ds)
                tcc_error("missing emutls descriptor for __thread '%s'",
                          get_tok_str(s->v, NULL));
            fn = external_global_sym(tok_alloc_const("__emutls_get_address"),
                                     &func_old_type);
            vpushsym(&func_old_type, fn);       /* the function    */
            vpushsym(&char_pointer_type, ds);   /* arg: &descriptor */
            gfunc_call(1);                      /* rax = T*         */
            /* gfunc_call pops func+args; push a fresh slot for the result */
            vpushi(0);
            vtop->type = s->type;
            /* Strip the __thread marker; the storage is already materialized. */
            vtop->type.t &= ~VT_TLS;
            if (s->type.t & VT_ARRAY) {
                /* Array: `rax` IS the per-thread base address, so no VT_LVAL
                   (the array must not be dereferenced first).  Treated like a
                   symbol whose address lives in `rax`. */
                vtop->r = REG_IRET;
            } else {
                /* Scalar / struct: `[rax]` is the lvalue. */
                vtop->r = REG_IRET | VT_LVAL;
            }
            vtop->c.i = 0;
            vtop->sym = NULL;
        } else {
            r = s->r;
            /* A symbol that has a register is a local register variable,
               which starts out as VT_LOCAL value.  */
            if ((r & VT_VALMASK) < VT_CONST)
                r = (r & ~VT_VALMASK) | VT_LOCAL;

            vset(&s->type, r, s->c);
            /* Point to s as backpointer (even without r&VT_SYM).
               Will be used by at least the x86 inline asm parser for
               regvars.  */
            vtop->sym = s;

            if (r & VT_SYM) {
                vtop->c.i = 0;
#ifdef TCC_TARGET_PE
                if (s->a.dllimport) {
                    mk_pointer(&vtop->type);
                    vtop->r |= VT_LVAL;
                    indir();
                }
#endif
            } else if (r == VT_CONST && IS_ENUM_VAL(s->type.t)) {
                vtop->c.i = s->enum_val;
            }
        }
        break;
    }
    
    /* post operations */
    while (1) {
        if (tok == TOK_INC || tok == TOK_DEC) {
            inc(1, tok);
            next();
        } else if (tok == '.' || tok == TOK_ARROW) {
            int qualifiers, cumofs;
            /* field */ 
            if (tok == TOK_ARROW) 
                indir();
            qualifiers = vtop->type.t & (VT_CONSTANT | VT_VOLATILE);
            test_lvalue();
            /* expect pointer on structure */
            next();
	    s = find_field(&vtop->type, tok | SYM_FIELD, &cumofs);
            if (!s && tok >= TOK_UIDENT) {
                /* C++ 式方法回退: v.func(…) → __method_<id>_func(&v, …) */
                int ident = tok;
                next();
                if (tok == '(' && method_lookup(ident))
                    continue;              /* tok = '(' → '(' 分支完成调用 */
                /* 回退: 恢复成员名, 走原字段报错路径 */
                s = find_field(&vtop->type, tok, &cumofs);
            }
            /* add field offset to pointer */
            gaddrof();
            vtop->type = char_pointer_type; /* change type to 'char *' */
            vpushi(cumofs);
            gen_op('+');
            /* change type to field type, and set to lvalue */
            vtop->type = s->type;
            if (qualifiers)
                parse_btype_qualify(&vtop->type, qualifiers);
            /* an array is never an lvalue */
            if (!(vtop->type.t & VT_ARRAY)) {
                vtop->r |= VT_LVAL;
#ifdef CONFIG_TCC_BCHECK
                /* if bound checking, the referenced pointer must be checked */
                if (tcc_state->do_bounds_check)
                    vtop->r |= VT_MUSTBOUND;
#endif
            }
            next();
        } else if (tok == '[') {
            next();
            gexpr();
            gen_op('+');
            indir();
            skip(']');
        } else if (tok == '(') {
            SValue ret;
            Sym *sa;
            int nb_args, ret_nregs, ret_align, regsize, variadic;
            TokenString *p, *p2;
            int method_self = method_pending_self;

            /* function call  */
            if (method_self) {
                /* 对象方法: 交换使 vtop = func 供类型检查, 下面取回 self */
                method_pending_self = 0;
                vswap();
            }
            if ((vtop->type.t & VT_BTYPE) != VT_FUNC) {
                /* pointer test (no array accepted) */
                if ((vtop->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR) {
                    vtop->type = *pointed_type(&vtop->type);
                    if ((vtop->type.t & VT_BTYPE) != VT_FUNC)
                        goto error_func;
                } else {
                error_func:
                    expect("function pointer");
                }
            } else {
                vtop->r &= ~VT_LVAL; /* no lvalue */
            }
            /* get return type */
            s = vtop->type.ref;
            next();
            sa = s->next; /* first parameter */
            nb_args = regsize = 0;
            if (method_self) {
                /* 还原 [func, self], vtop = self (self 已入参) */
                vswap();
                nb_args = 1;
                if (sa)
                    sa = sa->next;
            }
            ret.r2 = VT_CONST;
            /* compute first implicit argument if a structure is returned */
            if ((s->type.t & VT_BTYPE) == VT_STRUCT) {
                variadic = (s->f.func_type == FUNC_ELLIPSIS);
                ret_nregs = gfunc_sret(&s->type, variadic, &ret.type,
                                       &ret_align, &regsize);
                if (ret_nregs <= 0) {
                    /* get some space for the returned structure */
                    size = type_size(&s->type, &align);
#ifdef TCC_TARGET_ARM64
                /* On arm64, a small struct is return in registers.
                   It is much easier to write it to memory if we know
                   that we are allowed to write some extra bytes, so
                   round the allocated space up to a power of 2: */
                if (size < 16)
                    while (size & (size - 1))
                        size = (size | (size - 1)) + 1;
#endif
                    loc = (loc - size) & -align;
                    ret.type = s->type;
                    ret.r = VT_LOCAL | VT_LVAL;
                    /* pass it as 'int' to avoid structure arg passing
                       problems */
                    vseti(VT_LOCAL, loc);
#ifdef CONFIG_TCC_BCHECK
                    if (tcc_state->do_bounds_check)
                        --loc;
#endif
                    ret.c = vtop->c;
                    if (ret_nregs < 0)
                      vtop--;
                    else
                      nb_args++;
                }
            } else {
                ret_nregs = 1;
                ret.type = s->type;
            }

            if (ret_nregs > 0) {
                /* return in register */
                ret.c.i = 0;
                PUT_R_RET(&ret, ret.type.t);
            }

            p = NULL;
            if (tok != ')') {
                r = tcc_state->reverse_funcargs;
                for(;;) {
                    if (r) {
                        skip_or_save_block(&p2);
                        p2->prev = p, p = p2;
                    } else {
                        expr_eq();
                        gfunc_param_typed(s, sa);
                    }
                    nb_args++;
                    if (sa)
                        sa = sa->next;
                    if (tok == ')')
                        break;
                    skip(',');
                }
            }
            if (sa)
                tcc_error("too few arguments to function");

            if (p) { /* with reverse_funcargs */
                for (n = 0; p; p = p2, ++n) {
                    p2 = p, sa = s;
                    do {
                        sa = sa->next, p2 = p2->prev;
                    } while (p2 && sa);
                    p2 = p->prev;
                    begin_macro(p, 1), next();
                    expr_eq();
                    gfunc_param_typed(s, sa);
                    end_macro();
                }
                vrev(n);
            }

            next();
            vcheck_cmp(); /* the generators don't like VT_CMP on vtop */
            gfunc_call(nb_args);

            if (ret_nregs < 0) {
                vsetc(&ret.type, ret.r, &ret.c);
#ifdef TCC_TARGET_RISCV64
                arch_transfer_ret_regs(1);
#endif
            } else {
                /* return value */
                n = ret_nregs;
                while (n > 1) {
                    int rc = reg_classes[ret.r] & ~(RC_INT | RC_FLOAT);
                    /* We assume that when a structure is returned in multiple
                       registers, their classes are consecutive values of the
                       suite s(n) = 2^n */
                    rc <<= --n;
                    for (r = 0; r < NB_REGS; ++r)
                        if (reg_classes[r] & rc)
                            break;
                    vsetc(&ret.type, r, &ret.c);
                }
                vsetc(&ret.type, ret.r, &ret.c);
                vtop->r2 = ret.r2;
                /* mark direct pointer call returns as possibly-NULL so that a
                   same-expression dereference (-> * []) warns via -Wnonnull-deref */
                if ((ret.type.t & VT_BTYPE) == VT_PTR)
                    vtop->r |= VT_MAYNULL;

                /* handle packed struct return */
                if (((s->type.t & VT_BTYPE) == VT_STRUCT) && ret_nregs) {
                    int addr, offset;

                    size = type_size(&s->type, &align);
                    /* We're writing whole regs often, make sure there's enough
                       space.  Assume register size is power of 2.  */
                    size = (size + regsize - 1) & -regsize;
                    if (ret_align > align)
                        align = ret_align;
                    loc = (loc - size) & -align;
                    addr = loc;
                    offset = 0;
                    for (;;) {
                        vset(&ret.type, VT_LOCAL | VT_LVAL, addr + offset);
                        vswap();
                        vstore();
                        vtop--;
                        if (--ret_nregs == 0)
                          break;
                        offset += regsize;
                    }
                    vset(&s->type, VT_LOCAL | VT_LVAL, addr);
                }

                /* Promote char/short return values. This is matters only
                   for calling function that were not compiled by TCC and
                   only on some architectures.  For those where it doesn't
                   matter we expect things to be already promoted to int,
                   but not larger.  */
                t = s->type.t & VT_BTYPE;
                if (t == VT_BYTE || t == VT_SHORT || t == VT_BOOL) {
#ifdef PROMOTE_RET
                    vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
                    vtop->type.t = VT_INT;
#endif
                }
            }
            if (s->f.func_noreturn) {
                if (debug_modes)
	            tcc_tcov_block_end(tcc_state, -1);
                CODE_OFF();
	    }
        } else {
            break;
        }
    }
}

#ifndef precedence_parser /* original top-down parser */

static void expr_prod(void)
{
    int t;

    unary();
    while ((t = tok) == '*' || t == '/' || t == '%') {
        next();
        unary();
        gen_op(t);
    }
}

static void expr_sum(void)
{
    int t;

    expr_prod();
    while ((t = tok) == '+' || t == '-') {
        next();
        expr_prod();
        gen_op(t);
    }
}

static void expr_shift(void)
{
    int t;

    expr_sum();
    while ((t = tok) == TOK_SHL || t == TOK_SAR) {
        next();
        expr_sum();
        gen_op(t);
    }
}

static void expr_cmp(void)
{
    int t;

    expr_shift();
    while (((t = tok) >= TOK_ULE && t <= TOK_GT) ||
           t == TOK_ULT || t == TOK_UGE) {
        next();
        expr_shift();
        gen_op(t);
    }
}

static void expr_cmpeq(void)
{
    int t;

    expr_cmp();
    while ((t = tok) == TOK_EQ || t == TOK_NE) {
        next();
        expr_cmp();
        gen_op(t);
    }
}

static void expr_and(void)
{
    expr_cmpeq();
    while (tok == '&') {
        next();
        expr_cmpeq();
        gen_op('&');
    }
}

static void expr_xor(void)
{
    expr_and();
    while (tok == '^') {
        next();
        expr_and();
        gen_op('^');
    }
}

static void expr_or(void)
{
    expr_xor();
    while (tok == '|') {
        next();
        expr_xor();
        gen_op('|');
    }
}

static void expr_landor(int op);

static void expr_land(void)
{
    expr_or();
    if (tok == TOK_LAND)
        expr_landor(tok);
}

static void expr_lor(void)
{
    expr_land();
    if (tok == TOK_LOR)
        expr_landor(tok);
}

# define expr_landor_next(op) op == TOK_LAND ? expr_or() : expr_land()
#else /* defined precedence_parser */
# define expr_landor_next(op) unary(), expr_infix(precedence(op) + 1)
# define expr_lor() unary(), expr_infix(1)

static int precedence(int tok)
{
    switch (tok) {
        case TOK_LOR: return 1;
        case TOK_LAND: return 2;
	case '|': return 3;
	case '^': return 4;
	case '&': return 5;
	case TOK_EQ: case TOK_NE: return 6;
 relat: case TOK_ULT: case TOK_UGE: return 7;
	case TOK_SHL: case TOK_SAR: return 8;
	case '+': case '-': return 9;
	case '*': case '/': case '%': return 10;
	default:
	    if (tok >= TOK_ULE && tok <= TOK_GT)
	        goto relat;
	    return 0;
    }
}
static unsigned char prec[256];
static void init_prec(void)
{
    int i;
    for (i = 0; i < 256; i++)
	prec[i] = precedence(i);
}
#define precedence(i) ((unsigned)i < 256 ? prec[i] : 0)

static void expr_landor(int op);

static void expr_infix(int p)
{
    int t = tok, p2;
    while ((p2 = precedence(t)) >= p) {
        if (t == TOK_LOR || t == TOK_LAND) {
            expr_landor(t);
        } else {
            next();
            unary();
            if (precedence(tok) > p2)
              expr_infix(p2 + 1);
            gen_op(t);
        }
        t = tok;
    }
}
#endif

/* Assuming vtop is a value used in a conditional context
   (i.e. compared with zero) return 0 if it's false, 1 if
   true and -1 if it can't be statically determined.  */
static int condition_3way(void)
{
    int c = -1;
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
	(!(vtop->r & VT_SYM) || !vtop->sym->a.weak)) {
	vdup();
        gen_cast_s(VT_BOOL);
	c = vtop->c.i;
	vpop();
    }
    return c;
}

static void expr_landor(int op)
{
    int t = 0, cc = 1, f = 0, i = op == TOK_LAND, c;
    for(;;) {
        c = f ? i : condition_3way();
        if (c < 0)
            save_regs(1), cc = 0;
        else if (c != i)
            nocode_wanted++, f = 1;
        if (tok != op)
            break;
        if (c < 0)
            t = gvtst(i, t);
        else
            vpop();
        next();
        expr_landor_next(op);
    }
    if (cc || f) {
        vpop();
        vpushi(i ^ f);
        gsym(t);
        nocode_wanted -= f;
    } else {
        gvtst_set(i, t);
    }
}

static int is_cond_bool(SValue *sv)
{
    if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
        && (sv->type.t & VT_BTYPE) == VT_INT)
        return (unsigned)sv->c.i < 2;
    if (sv->r == VT_CMP)
        return 1;
    return 0;
}

static void expr_cond(void)
{
    int tt, u, r1, r2, rc, t1, t2, islv, c, g;
    SValue sv;
    CType type;

    expr_lor();
    if (tok == '?') {
        next();
	c = condition_3way();
        g = (tok == ':' && gnu_ext);
        tt = 0;
        if (!g) {
            if (c < 0) {
                save_regs(1);
                tt = gvtst(1, 0);
            } else {
                vpop();
            }
        } else if (c < 0) {
            /* needed to avoid having different registers saved in
               each branch */
            save_regs(1);
            gv_dup();
            tt = gvtst(0, 0);
        }

        if (c == 0)
          nocode_wanted++;
        if (!g)
          gexpr();

        if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
          mk_pointer(&vtop->type);
        sv = *vtop; /* save value to handle it later */
        vtop--; /* no vpop so that FP stack is not flushed */

        if (g) {
            u = tt;
        } else if (c < 0) {
            u = gjmp(0);
            gsym(tt);
        } else
          u = 0;

        if (c == 0)
          nocode_wanted--;
        if (c == 1)
          nocode_wanted++;
        skip(':');
        expr_cond();

        if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
          mk_pointer(&vtop->type);

        /* cast operands to correct type according to ISOC rules */
        if (!combine_types(&type, &sv, vtop, '?'))
          type_incompatibility_error(&sv.type, &vtop->type,
            "type mismatch in conditional expression (have '%s' and '%s')");

        if (c < 0 && is_cond_bool(vtop) && is_cond_bool(&sv)) {
            /* optimize "if (f ? a > b : c || d) ..." for example, where normally
               "a < b" and "c || d" would be forced to "(int)0/1" first, whereas
               this code jumps directly to the if's then/else branches. */
            t1 = gvtst(0, 0);
            t2 = gjmp(0);
            gsym(u);
            vpushv(&sv);
            /* combine jump targets of 2nd op with VT_CMP of 1st op */
            gvtst_set(0, t1);
            gvtst_set(1, t2);
            gen_cast(&type);
            //  tcc_warning("two conditions expr_cond");
            return;
        }

        /* keep structs lvalue by transforming `(expr ? a : b)` to `*(expr ? &a : &b)` so
           that `(expr ? a : b).mem` does not error  with "lvalue expected" */
        islv = VT_STRUCT == (type.t & VT_BTYPE);

        /* now we convert second operand */
        if (c != 1) {
            gen_cast(&type);
            if (islv) {
                mk_pointer(&vtop->type);
                gaddrof();
            }
        }

        rc = RC_TYPE(type.t);
        /* for long longs, we use fixed registers to avoid having
           to handle a complicated move */
        if (USING_TWO_WORDS(type.t))
          rc = RC_RET(type.t);

        tt = r2 = 0;
        if (c < 0) {
            if (type.t != VT_VOID)
                r2 = gv(rc);
            tt = gjmp(0);
        }
        gsym(u);
        if (c == 1)
          nocode_wanted--;

        /* this is horrible, but we must also convert first
           operand */
        if (c != 0) {
            *vtop = sv;
            gen_cast(&type);
            if (islv) {
                mk_pointer(&vtop->type);
                gaddrof();
            }
        }

        if (c < 0) {
            if (type.t != VT_VOID) {
                r1 = gv(rc);
                move_reg(r2, r1, islv ? VT_PTR : type.t);
                vtop->r = r2;
            }
            gsym(tt);
        }

        if (islv)
          indir();
    }
}

static void expr_eq(void)
{
    int t;
    
    expr_cond();
    if ((t = tok) == '=' || TOK_ASSIGN(t)) {
        test_lvalue();
        next();
        if (t == '=') {
            expr_eq();
        } else {
            vdup();
            expr_eq();
            gen_op(TOK_ASSIGN_OP(t));
        }
        vstore();
    }
}

ST_FUNC void gexpr(void)
{
    expr_eq();
    if (tok == ',') {
        do {
            vpop();
            next();
            expr_eq();
        } while (tok == ',');

        /* convert array & function to pointer */
        convert_parameter_type(&vtop->type);

        /* make builtin_constant_p((1,2)) return 0 (like on gcc) */
        if ((vtop->r & VT_VALMASK) == VT_CONST && nocode_wanted && !CONST_WANTED)
            if (vtop->type.t != VT_VOID && (vtop->type.t & VT_BTYPE) != VT_STRUCT)
                gv(RC_TYPE(vtop->type.t));
    }
}

/* parse a constant expression and return value in vtop.  */
static void expr_const1(void)
{
    nocode_wanted += CONST_WANTED_BIT;
    expr_cond();
    nocode_wanted -= CONST_WANTED_BIT;
}

/* parse an integer constant and return its value. */
static inline int64_t expr_const64(void)
{
    int64_t c;
    expr_const1();
    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST)
        expect("constant expression");
    c = vtop->c.i;
    vpop();
    return c;
}

/* parse an integer constant and return its value.
   Complain if it doesn't fit 32bit (signed or unsigned).  */
ST_FUNC int expr_const(void)
{
    int c;
    int64_t wc = expr_const64();
    c = wc;
    if (c != wc && (unsigned)c != wc)
        tcc_error("constant exceeds 32 bit");
    return c;
}

/* ------------------------------------------------------------------------- */
/* return from function */

#ifndef TCC_TARGET_ARM64
static void gfunc_return(CType *func_type)
{
    if ((func_type->t & VT_BTYPE) == VT_STRUCT) {
        CType type, ret_type;
        int ret_align, ret_nregs, regsize;
        ret_nregs = gfunc_sret(func_type, func_var, &ret_type,
                               &ret_align, &regsize);
        if (ret_nregs < 0) {
#ifdef TCC_TARGET_RISCV64
            arch_transfer_ret_regs(0);
#endif
        } else if (0 == ret_nregs) {
            /* if returning structure, must copy it to implicit
               first pointer arg location */
            type = *func_type;
            mk_pointer(&type);
            vset(&type, VT_LOCAL | VT_LVAL, func_vc);
            indir();
            vswap();
            /* copy structure value to pointer */
            vstore();
        } else {
            /* returning structure packed into registers */
            int size, addr, align, rc, n;
            size = type_size(func_type,&align);
            if (ret_nregs * regsize > size ||
		((align & (ret_align - 1))
                 && ((vtop->r & VT_VALMASK) < VT_CONST /* pointer to struct */
                     || (vtop->c.i & (ret_align - 1))
                     ))) {
		if (ret_nregs * regsize > size)
		    size = ret_nregs * regsize;
		if (ret_align > align)
		    align = ret_align;
                loc = (loc - size) & -align;
                addr = loc;
                type = *func_type;
                vset(&type, VT_LOCAL | VT_LVAL, addr);
                vswap();
                vstore();
                vpop();
                vset(&ret_type, VT_LOCAL | VT_LVAL, addr);
            }
            vtop->type = ret_type;
            rc = RC_RET(ret_type.t);
            //printf("struct return: n:%d t:%02x rc:%02x\n", ret_nregs, ret_type.t, rc);
            for (n = ret_nregs; --n > 0;) {
                vdup();
                gv(rc);
                vswap();
                incr_offset(regsize);
                /* We assume that when a structure is returned in multiple
                   registers, their classes are consecutive values of the
                   suite s(n) = 2^n */
                rc <<= 1;
            }
            gv(rc);
            vtop -= ret_nregs - 1;
        }
    } else {
        gv(RC_RET(func_type->t));
    }
    vtop--; /* NOT vpop() because on x86 it would flush the fp stack */
}
#endif

static void check_func_return(void)
{
    if ((func_vt.t & VT_BTYPE) == VT_VOID)
        return;
    if ((!strcmp(funcname, "main") || func_old)
        && (func_vt.t & VT_BTYPE) == VT_INT) {
        /* main returns 0 by default */
        vpushi(0);
        gfunc_return(&func_vt);
    } else {
        tcc_warning("function might return no value: '%s'", funcname);
    }
}

/* ------------------------------------------------------------------------- */
/* switch/case */

static int case_cmp(uint64_t a, uint64_t b)
{
    if (cur_switch->sv.type.t & VT_UNSIGNED)
        return a < b ? -1 : a > b;
    else
        return (int64_t)a < (int64_t)b ? -1 : (int64_t)a > (int64_t)b;
}

static int case_cmp_qs(const void *pa, const void *pb)
{
    return case_cmp((*(struct case_t**)pa)->v1, (*(struct case_t**)pb)->v1);
}

static void case_sort(struct switch_t *sw)
{
    struct case_t **p;
    if (sw->n < 2)
        return;
    qsort(sw->p, sw->n, sizeof *sw->p, case_cmp_qs);
    p = sw->p;
    while (p < sw->p + sw->n - 1) {
        if (case_cmp(p[0]->v2, p[1]->v1) >= 0) {
            int l1 = p[0]->line, l2 = p[1]->line;
            /* using special format "%i:..." to show specific line */
            tcc_error("%i:duplicate case value", l1 > l2 ? l1 : l2);
        } else if (p[0]->v2 + 1 == p[1]->v1 && p[0]->ind == p[1]->ind) {
            /* treat "case 1: case 2: case 3:" like "case 1 ... 3: */
            p[1]->v1 = p[0]->v1;
            tcc_free(p[0]);
            memmove(p, p + 1, (--sw->n - (p - sw->p)) * sizeof *p);
        } else
            ++p;
    }
}

static int gcase(struct case_t **base, int len, int dsym)
{
    struct case_t *p;
    int t, l2, e;

    t = vtop->type.t & VT_BTYPE;
    if (t != VT_LLONG)
        t = VT_INT;
    while (len) {
        /* binary search while len > 8, else linear */
        l2 = len > 8 ? len/2 : 0;
        p = base[l2];
        vdup(), vpush64(t, p->v2);
        if (l2 == 0 && p->v1 == p->v2) {
            gen_op(TOK_EQ); /* jmp to case when equal */
            gsym_addr(gvtst(0, 0), p->ind);
        } else {
            /* case v1 ... v2 */
            gen_op(TOK_GT); /* jmp over when > V2 */
            if (len == 1) /* last case test jumps to default when false */
                dsym = gvtst(0, dsym), e = 0;
            else
                e = gvtst(0, 0);
            vdup(), vpush64(t, p->v1);
            gen_op(TOK_GE); /* jmp to case when >= V1 */
            gsym_addr(gvtst(0, 0), p->ind);
            dsym = gcase(base, l2, dsym);
            gsym(e);
        }
        ++l2, base += l2, len -= l2;
    }
    /* jump automagically will suppress more jumps */
    return gjmp(dsym);
}

static void end_switch(void)
{
    struct switch_t *sw = cur_switch;
    dynarray_reset(&sw->p, &sw->n);
    cur_switch = sw->prev;
    tcc_free(sw);
}

/* ------------------------------------------------------------------------- */
/* __attribute__((cleanup(fn))) */

/* protect symbol lvalues from further modification  */
static void save_lvalues(void)
{
    SValue *sv = vtop;
    while (sv >= vstack) {
        if (sv->sym && (sv->r & VT_LVAL)) {
            int align, size = type_size(&sv->type, &align);
            int r2, l = get_temp_local_var(size, align, &r2);
            vset(&sv->type, VT_LOCAL | VT_LVAL, l), vtop->r2 = r2;
            vpushv(sv), *sv = vtop[-1], vstore(), --vtop;
        }
        --sv;
    }
}

static void try_call_scope_cleanup(Sym *stop)
{
    Sym *cls = cur_scope->cl.s;
    for (; cls != stop; cls = cls->next) {
	Sym *fs = cls->cleanup_func;
	Sym *vs = cls->cleanup_sym;
	save_lvalues();
	if (fs->v & SYM_DEFER) {
	    /* defer 记录: 重放保存的参数并调用 (cleanup_func 的 c 存 desc 索引) */
	    emit_defer_call(&defer_descs[fs->c]);
	} else {
	    vpushsym(&fs->type, fs);
	    vset(&vs->type, vs->r, vs->c);
	    vtop->sym = vs;
	    mk_pointer(&vtop->type);
	    gaddrof();
	    gfunc_call(1);
	}
    }
}

/* defer 语句: 注册点求值参数并保存到函数帧上的记录区 D;
   离开作用域时由 scope cleanup 机制逆序重放调用 (Go 式语义)。
   语法: defer func(args...); */
static void defer_statement(void)
{
    int t, n, i, size, align, max_align, off, r;
    Sym *s, *sa, *dummy;
    DeferDesc *dd;

    if (!funcname[0] || !local_stack)
        tcc_error("defer outside of function");
    if (nb_defer_descs >= MAX_DEFER_DESC)
        tcc_error("too many defer statements in function");

    t = tok;
    if (t < TOK_UIDENT)
        tcc_error("defer requires a function call");
    next();
    s = sym_find(t);
    if (!s || (s->type.t & VT_BTYPE) != VT_FUNC)
        tcc_error("defer requires a function call");

    dd = &defer_descs[nb_defer_descs];
    dd->fn = s;
    dd->nargs = 0;

    /* 函数符号压栈 (与 unary 调用一致) */
    r = s->r;
    if ((r & VT_VALMASK) < VT_CONST)
        r = (r & ~VT_VALMASK) | VT_LOCAL;
    vset(&s->type, r, s->c);
    vtop->sym = s;

    n = 0;
    if (tok == '(') {
        next();
        sa = s->type.ref->next;   /* 第一原型参数 */
        if (tok != ')') for (;;) {
            expr_eq();
            /* 原型/旧式参数类型转换 (与普通调用一致) */
            gfunc_param_typed(s, sa);
            if (sa)
                sa = sa->next;
            if (vtop->type.t & (VT_VLA | VT_ARRAY))
                tcc_error("defer: variable-length or array argument not supported");
            if (n >= MAX_DEFER_ARGS)
                tcc_error("defer: too many arguments (max %d)", MAX_DEFER_ARGS);
            dd->argtypes[n] = vtop->type;
            n++;
            if (tok == ')')
                break;
            skip(',');
        }
        skip(')');
    }
    dd->nargs = n;
    if (tok != ';')
        tcc_error("defer requires a function call");
    next();

    /* 计算记录区 D 布局: 参数按类型对齐连续存放 */
    off = 0;
    max_align = 1;
    for (i = 0; i < n; i++) {
        size = type_size(&dd->argtypes[i], &align);
        off = (off + align - 1) & -align;
        dd->argoffs[i] = off;
        off += size;
        if (align > max_align)
            max_align = align;
    }
    off = (off + max_align - 1) & -max_align;
    /* D 以普通局部变量方式分配在函数帧 (不被临时变量复用机制回收) */
    loc = (loc - off) & -max_align;
    dd->d_off = loc;

    /* 参数值从后往前存入 D (vstack: [fn, arg1..argN], vtop=argN;
       fn 是替换语句前栈顶而非压栈, store 后深度已回语句前, 不弹) */
    for (i = n - 1; i >= 0; i--) {
        SValue save = *vtop;
        vset(&dd->argtypes[i], VT_LOCAL | VT_LVAL, dd->d_off + dd->argoffs[i]);
        vpushv(&save);
        vstore();
        vpop();   /* 弹值槽 */
        vpop();   /* 弹 arg_i 槽 → 下移到 arg_{i-1} */
    }
    vpop();   /* 弹函数符号, 语句前后深度一致 */

    /* 挂到当前作用域 cleanup 链; cleanup_func 用伪 Sym 标记 defer,
       cl 链自身 v 保持 SYM_FIELD|n 干净 (不影响 goto 跨块编号比较) */
    dummy = sym_push2(&all_cleanups, SYM_FIELD | SYM_DEFER, 0, 0);
    dummy->type.t = VT_VOID;
    dummy->c = nb_defer_descs;   /* desc 索引 */
    {
        Sym *cls = sym_push2(&all_cleanups, SYM_FIELD | ++cur_scope->cl.n, 0, 0);
        cls->cleanup_func = dummy;
        cls->next = cur_scope->cl.s;
        cur_scope->cl.s = cls;
    }
    nb_defer_descs++;
}

/* 在作用域退出点重放 defer 调用: load 记录区参数并调用 (逆序由 cl 链保证) */
static void emit_defer_call(DeferDesc *dd)
{
    int i;
    CValue cv;
    /* 函数符号压栈 (符号引用, 无副作用) */
    vpushsym(&dd->fn->type, dd->fn);
    /* 参数从左到右压槽并指向 D; gfunc_call 期望 vtop = 最右参数,
       压栈顺序天然满足 (与 unary 的 reverse_funcargs+vrev 殊途同归) */
    for (i = 0; i < dd->nargs; i++) {
        cv.i = dd->d_off + dd->argoffs[i];
        vsetc(&dd->argtypes[i], VT_LOCAL | VT_LVAL, &cv);
    }
    gfunc_call(dd->nargs);
}


static void try_call_cleanup_goto(Sym *cleanupstate)
{
    Sym *oc, *cc;
    int ocd, ccd;

    if (!cur_scope->cl.s)
	return;

    /* search NCA of both cleanup chains given parents and initial depth */
    ocd = cleanupstate ? cleanupstate->v & ~SYM_FIELD : 0;
    for (ccd = cur_scope->cl.n, oc = cleanupstate; ocd > ccd; --ocd, oc = oc->next)
      ;
    for (cc = cur_scope->cl.s; ccd > ocd; --ccd, cc = cc->next)
      ;
    for (; cc != oc; cc = cc->next, oc = oc->next, --ccd)
      ;

    try_call_scope_cleanup(cc);
}

/* call 'func' for each __attribute__((cleanup(func))) */
static void block_cleanup(struct scope *o)
{
    int jmp = 0;
    Sym *g, **pg;
    for (pg = &pending_gotos; (g = *pg) && g->c > o->cl.n;) {
        if (g->cleanup_label->r & LABEL_FORWARD) {
            Sym *pcl = g->next;
            if (!jmp)
                jmp = gjmp(0);
            gsym(pcl->jnext);
            try_call_scope_cleanup(o->cl.s);
            pcl->jnext = gjmp(0);
            if (!o->cl.n)
                goto remove_pending;
            g->c = o->cl.n;
            pg = &g->prev;
        } else {
    remove_pending:
            *pg = g->prev;
            sym_free(g);
        }
    }
    gsym(jmp);
    try_call_scope_cleanup(o->cl.s);
}

/* ------------------------------------------------------------------------- */
/* VLA */

static void vla_restore(int loc)
{
    if (loc)
        gen_vla_sp_restore(loc);
}

static void vla_leave(struct scope *o)
{
    struct scope *c = cur_scope, *v = NULL;
    for (; c != o && c; c = c->prev)
      if (c->vla.num)
        v = c;
    if (v)
      vla_restore(v->vla.locorig);
}

/* ------------------------------------------------------------------------- */
/* local scopes */

static void new_scope(struct scope *o)
{
    /* copy and link previous scope */
    *o = *cur_scope;
    o->prev = cur_scope;
    cur_scope = o;
    cur_scope->vla.num = 0;

    /* record local declaration stack position */
    o->lstk = local_stack;
    o->llstk = local_label_stack;
    ++local_scope;
}

static void prev_scope(struct scope *o, int is_expr)
{
    vla_leave(o->prev);

    if (o->cl.s != o->prev->cl.s)
        block_cleanup(o->prev);

    if (debug_modes)
        tcc_debug_end_scope(o->lstk, !is_expr);

    /* pop locally defined labels */
    label_pop(&local_label_stack, o->llstk, is_expr);

    /* In the is_expr case (a statement expression is finished here),
       vtop might refer to symbols on the local_stack.  Either via the
       type or via vtop->sym.  We can't pop those nor any that in turn
       might be referred to.  To make it easier we don't roll back
       any symbols in that case; some upper level call to block() will
       do that.  We do have to remove such symbols from the lookup
       tables, though.  sym_pop will do that.  */

    /* pop locally defined symbols */
    sym_pop(&local_stack, o->lstk, is_expr);
    cur_scope = o->prev;
    --local_scope;
}

/* leave a scope via break/continue(/goto) */
static void leave_scope(struct scope *o)
{
    if (!o)
        return;
    try_call_scope_cleanup(o->cl.s);
    vla_leave(o);
}

/* short versiona for scopes with 'if/do/while/switch' which can
   declare only types (of struct/union/enum) */
static void new_scope_s(struct scope *o)
{
    o->lstk = local_stack;
    ++local_scope;
}

static void prev_scope_s(struct scope *o)
{
    sym_pop(&local_stack, o->lstk, 0);
    --local_scope;
}

/* ------------------------------------------------------------------------- */
/* call block from 'for do while' loops */

static void lblock(int *bsym, int *csym)
{
    struct scope *lo = loop_scope, *co = cur_scope;
    int *b = co->bsym, *c = co->csym;
    if (csym) {
        co->csym = csym;
        loop_scope = co;
    }
    co->bsym = bsym;
    block(0);
    co->bsym = b;
    if (csym) {
        co->csym = c;
        loop_scope = lo;
    }
}

/* c2y if/switch declaration */
static void gexpr_decl(void)
{
    int v = decl(VT_JMP);
    if (v > 1 && tok != ';') {
        Sym *s = sym_find(v);
        vset(&s->type, s->r, (s->r & VT_SYM) ? 0 : s->c);
        vtop->sym = s;
    } else {
        if (v)
            skip(';');
        gexpr();
    }
}

static void block(int flags)
{
    int a, b, c, d, e, t;
    struct scope o;
    Sym *s;

again:
    t = tok;
    /* If the token carries a value, next() might destroy it. Only with
       invalid code such as f(){"123"4;} */
    if (TOK_HAS_VALUE(t))
        goto expr;
    next();

    if (debug_modes)
        tcc_tcov_check_line (tcc_state, 0), tcc_tcov_block_begin (tcc_state);

    if (t == TOK_IF) {
        new_scope_s(&o);
        skip('(');
        gexpr_decl();
        a = gvtst(1, 0);
        skip(')');
        block(0);
        if (tok == TOK_ELSE) {
            d = gjmp(0);
            gsym(a);
            next();
            block(0);
            gsym(d); /* patch else jmp */
        } else {
            gsym(a);
        }
        prev_scope_s(&o);

    } else if (t == TOK_WHILE) {
        new_scope_s(&o);
        d = gind();
        skip('(');
        gexpr();
        a = gvtst(1, 0);
        skip(')');
        b = 0;
        lblock(&a, &b);
        gjmp_addr(d);
        gsym_addr(b, d);
        gsym(a);
        prev_scope_s(&o);

    } else if (t == '{') {
        if (debug_modes)
            tcc_debug_stabn(tcc_state, N_LBRAC, ind - func_ind);
        new_scope(&o);

        /* handle local labels declarations */
        while (tok == TOK_LABEL) {
            do {
                next();
                if (tok < TOK_UIDENT)
                    expect("label identifier");
                label_push(&local_label_stack, tok, LABEL_DECLARED);
                next();
            } while (tok == ',');
            skip(';');
        }

        while (tok != '}') {
	    decl(VT_LOCAL);
            if (tok != '}') {
                block(flags | STMT_COMPOUND);
            }
        }

        prev_scope(&o, flags & STMT_EXPR);
        if (debug_modes)
            tcc_debug_stabn(tcc_state, N_RBRAC, ind - func_ind);
        if (local_scope)
            next();
        else if (!nocode_wanted)
            check_func_return();

    } else if (t == TOK_DEFER) {
        defer_statement();

    } else if (t == TOK_RETURN) {
        b = (func_vt.t & VT_BTYPE) != VT_VOID;
        if (tok != ';') {
            gexpr();
            if (b) {
                gen_assign_cast(&func_vt);
            } else {
                if (vtop->type.t != VT_VOID)
                    tcc_warning("void function returns a value");
                vtop--;
            }
        } else if (b && func_old && (func_vt.t & VT_BTYPE) == VT_INT) {
            vpushi(0);
        } else if (b) {
            tcc_warning("'return' with no value");
            b = 0;
        }
        leave_scope(root_scope);
        if (b)
            gfunc_return(&func_vt);
        skip(';');
        /* jump unless last stmt in top-level block */
        if (tok != '}' || local_scope != 1)
            rsym = gjmp(rsym);
        if (debug_modes)
	    tcc_tcov_block_end (tcc_state, -1);
        CODE_OFF();

    } else if (t == TOK_BREAK) {
        /* compute jump */
        if (!cur_scope->bsym)
            tcc_error("cannot break");
        if (cur_switch && cur_scope->bsym == cur_switch->bsym)
            leave_scope(cur_switch->scope);
        else
            leave_scope(loop_scope);
        *cur_scope->bsym = gjmp(*cur_scope->bsym);
        skip(';');

    } else if (t == TOK_CONTINUE) {
        /* compute jump */
        if (!cur_scope->csym)
            tcc_error("cannot continue");
        leave_scope(loop_scope);
        *cur_scope->csym = gjmp(*cur_scope->csym);
        skip(';');

    } else if (t == TOK_FOR) {
        new_scope(&o);

        skip('(');
        if (tok != ';') {
            /* c99 for-loop init decl? */
            if (!decl(VT_JMP)) {
                /* no, regular for-loop init expr */
                gexpr();
                vpop();
            }
        }
        skip(';');
        a = b = 0;
        c = d = gind();
        if (tok != ';') {
            gexpr();
            a = gvtst(1, 0);
        }
        skip(';');
        if (tok != ')') {
            e = gjmp(0);
            d = gind();
            gexpr();
            vpop();
            gjmp_addr(c);
            gsym(e);
        }
        skip(')');
        lblock(&a, &b);
        gjmp_addr(d);
        gsym_addr(b, d);
        gsym(a);
        prev_scope(&o, 0);

    } else if (t == TOK_DO) {
        new_scope_s(&o);
        a = b = 0;
        d = gind();
        lblock(&a, &b);
        gsym(b);
        skip(TOK_WHILE);
        skip('(');
	gexpr();
        c = gvtst(0, 0);
        skip(')');
        skip(';');
	gsym_addr(c, d);
        gsym(a);
        prev_scope_s(&o);

    } else if (t == TOK_SWITCH) {
        struct switch_t *sw;

        sw = tcc_mallocz(sizeof *sw);
        sw->bsym = &a;
        sw->scope = cur_scope;
        sw->prev = cur_switch;
        sw->nocode_wanted = nocode_wanted;
        cur_switch = sw;

        new_scope_s(&o);
        skip('(');
        gexpr_decl();
        if (!is_integer_btype(vtop->type.t & VT_BTYPE))
            tcc_error("switch value not an integer");
        skip(')');
        sw->sv = *vtop--; /* save switch value */
        a = 0;
        b = gjmp(0); /* jump to first case */
        lblock(&a, NULL);
        a = gjmp(a); /* add implicit break */
        /* case lookup */
        gsym(b);
        prev_scope_s(&o);
        if (sw->nocode_wanted)
            goto skip_switch;
        case_sort(sw);
        sw->bsym = NULL; /* marker for 32bit:gen_opl() */
        vpushv(&sw->sv);
        gv(RC_INT);
        d = gcase(sw->p, sw->n, 0);
        vpop();
        if (sw->def_sym)
            gsym_addr(d, sw->def_sym);
        else
            gsym(d);
    skip_switch:
        /* break label */
        gsym(a);
        end_switch();

    } else if (t == TOK_CASE) {
        struct case_t *cr;
        if (!cur_switch)
            expect("switch");
        cr = tcc_malloc(sizeof(struct case_t));
        dynarray_add(&cur_switch->p, &cur_switch->n, cr);
        t = cur_switch->sv.type.t;
        cr->v1 = cr->v2 = value64(expr_const64(), t);
        if (tok == TOK_DOTS && gnu_ext) {
            next();
            cr->v2 = value64(expr_const64(), t);
            if (case_cmp(cr->v2, cr->v1) < 0)
                tcc_warning("empty case range");
        }
        /* case and default are unreachable from a switch under nocode_wanted */
        if (!cur_switch->nocode_wanted)
            cr->ind = gind();
        cr->line = file->line_num;
        skip(':');
        goto block_after_label;

    } else if (t == TOK_DEFAULT) {
        if (!cur_switch)
            expect("switch");
        if (cur_switch->def_sym)
            tcc_error("too many 'default'");
        cur_switch->def_sym = cur_switch->nocode_wanted ? -1 : gind();
        skip(':');
        goto block_after_label;

    } else if (t == TOK_GOTO) {
        vla_restore(cur_scope->vla.locorig);
        if (tok == '*' && gnu_ext) {
            /* computed goto */
            next();
            gexpr();
            if ((vtop->type.t & VT_BTYPE) != VT_PTR)
                expect("pointer");
            ggoto();

        } else if (tok >= TOK_UIDENT) {
	    s = label_find(tok);
	    /* put forward definition if needed */
            if (!s)
              s = label_push(&global_label_stack, tok, LABEL_FORWARD);
            else if (s->r == LABEL_DECLARED)
              s->r = LABEL_FORWARD;

	    if (s->r & LABEL_FORWARD) {
		/* start new goto chain for cleanups, linked via label->next */
		if (cur_scope->cl.s && !nocode_wanted) {
                    sym_push2(&pending_gotos, SYM_FIELD, 0, cur_scope->cl.n);
                    pending_gotos->cleanup_label = s;
                    s = sym_push2(&s->next, SYM_FIELD, 0, 0);
                    pending_gotos->next = s;
                }
		s->jnext = gjmp(s->jnext);
	    } else {
		try_call_cleanup_goto(s->cleanupstate);
		gjmp_addr(s->jind);
	    }
	    next();

        } else {
            expect("label identifier");
        }
        skip(';');

    } else if (t == TOK_ASM1 || t == TOK_ASM2 || t == TOK_ASM3) {
        asm_instr();

    } else {
        if (tok == ':' && t >= TOK_UIDENT) {
            /* label case */
	    next();
            s = label_find(t);
            if (s) {
                if (s->r == LABEL_DEFINED)
                    tcc_error("duplicate label '%s'", get_tok_str(s->v, NULL));
                s->r = LABEL_DEFINED;
		if (s->next) {
		    Sym *pcl; /* pending cleanup goto */
		    for (pcl = s->next; pcl; pcl = pcl->prev)
		      gsym(pcl->jnext);
		    sym_pop(&s->next, NULL, 0);
		} else
		  gsym(s->jnext);
            } else {
                s = label_push(&global_label_stack, t, LABEL_DEFINED);
            }
            s->jind = gind();
            s->cleanupstate = cur_scope->cl.s;

    block_after_label:
            /* Accept attributes after labels (e.g. 'unused') */
            parse_attribute(NULL);

            if (debug_modes)
                tcc_tcov_reset_ind(tcc_state);
            vla_restore(cur_scope->vla.loc);

            if (tok != '}') {
                if (0 == (flags & STMT_COMPOUND))
                    goto again;
                /* C23: insert implicit null-statement whithin compound statement */
            } else {
                /* we accept this, but it is a mistake */
                tcc_warning_c(warn_all)("deprecated use of label at end of compound statement");
            }
        } else {
            /* expression case */
            if (t != ';') {
                unget_tok(t);
    expr:
                if (flags & STMT_EXPR) {
                    vpop();
                    gexpr();
                } else {
                    gexpr();
                    vpop();
                }
                skip(';');
            }
        }
    }

    if (debug_modes)
        tcc_tcov_check_line (tcc_state, 0), tcc_tcov_block_end (tcc_state, 0);
}

/* This skips over a stream of tokens containing balanced {} and ()
   pairs, stopping at outer ',' ';' and '}' (or matching '}' if we started
   with a '{').  If STR then allocates and stores the skipped tokens
   in *STR.  This doesn't check if () and {} are nested correctly,
   i.e. "({)}" is accepted.  */
static void skip_or_save_block(TokenString **str)
{
    int braces = tok == '{';
    int level = 0;
    if (str)
      *str = tok_str_alloc();

    while (1) {
	int t = tok;
        if (level == 0
            && (t == ','
             || t == ';'
             || t == '}'
             || t == ')'
             || t == ']'))
             break;
	if (t == TOK_EOF) {
	     if (str || level > 0)
	       tcc_error("unexpected end of file");
	     else
	       break;
	}
	if (str)
	  tok_str_add_tok(*str);
	next();
	if (t == '{' || t == '(' || t == '[') {
	    level++;
	} else if (t == '}' || t == ')' || t == ']') {
	    level--;
	    if (level == 0 && braces && t == '}')
	      break;
	}
    }
    if (str)
	tok_str_add(*str, TOK_EOF);
}

#define EXPR_CONST 1
#define EXPR_ANY   2

static void parse_init_elem(int expr_type)
{
    int saved_global_expr;
    switch(expr_type) {
    case EXPR_CONST:
        /* compound literals must be allocated globally in this case */
        saved_global_expr = global_expr;
        global_expr = 1;
        expr_const1();
        global_expr = saved_global_expr;
        /* NOTE: symbols are accepted, as well as lvalue for anon symbols
	   (compound literals).  */
        if (((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST
             && ((vtop->r & (VT_SYM|VT_LVAL)) != (VT_SYM|VT_LVAL)
                 || vtop->sym->v < SYM_FIRST_ANOM))
#ifdef TCC_TARGET_PE
                 || ((vtop->r & VT_SYM) && vtop->sym->a.dllimport)
#endif
           )
            tcc_error("initializer element is not constant");
        break;
    case EXPR_ANY:
        expr_eq();
        break;
    }
}

#if 1
static void init_assert(init_params *p, int offset)
{
    if (p->sec ? !NODATA_WANTED && offset > p->sec->data_offset
               : !nocode_wanted && offset > p->local_offset)
        tcc_internal_error("initializer overflow");
}
#else
#define init_assert(sec, offset)
#endif

/* put zeros for variable based init */
static void init_putz(init_params *p, unsigned long c, int size)
{
    init_assert(p, c + size);
    if (p->sec) {
        /* nothing to do because globals are already set to zero */
    } else {
        vpush_helper_func(TOK_memset);
        vseti(VT_LOCAL, c);
        vpushi(0);
        vpushs(size);
#if defined TCC_TARGET_ARM && defined TCC_ARM_EABI
        vswap();  /* using __aeabi_memset(void*, size_t, int) */
#endif
        gfunc_call(3);
    }
}

#define DIF_FIRST     1
#define DIF_SIZE_ONLY 2
#define DIF_HAVE_ELEM 4
#define DIF_CLEAR     8

/* delete relocations for specified range c ... c + size. Unfortunatly
   in very special cases, relocations may occur unordered */
static void decl_design_delrels(Section *sec, int c, int size)
{
    ElfW_Rel *rel, *rel2, *rel_end;
    if (!sec || !sec->reloc)
        return;
    rel = rel2 = (ElfW_Rel*)sec->reloc->data;
    rel_end = (ElfW_Rel*)(sec->reloc->data + sec->reloc->data_offset);
    while (rel < rel_end) {
        if (rel->r_offset >= c && rel->r_offset < c + size) {
            sec->reloc->data_offset -= sizeof *rel;
        } else {
            if (rel2 != rel)
                memcpy(rel2, rel, sizeof *rel);
            ++rel2;
        }
        ++rel;
    }
}

static void decl_design_flex(init_params *p, Sym *ref, int index)
{
    if (ref == p->flex_array_ref) {
        if (index >= ref->c)
            ref->c = index + 1;
    } else if (ref->c < 0 && index >= 0)
        tcc_error("flexible array has zero size in this context");
}

/* t is the array or struct type. c is the array or struct
   address. cur_field is the pointer to the current
   field, for arrays the 'c' member contains the current start
   index.  'flags' is as in decl_initializer.
   'al' contains the already initialized length of the
   current container (starting at c).  This returns the new length of that.  */
static int decl_designator(init_params *p, CType *type, unsigned long c,
                           Sym **cur_field, int flags, int al)
{
    Sym *s, *f;
    int index, index_last, align, l, nb_elems, elem_size;
    unsigned long corig = c;

    elem_size = 0;
    nb_elems = 1;

    if (flags & DIF_HAVE_ELEM)
        goto no_designator;

    if (gnu_ext && tok >= TOK_UIDENT) {
        l = tok, next();
        if (tok == ':')
            goto struct_field;
        unget_tok(l);
    }

    /* NOTE: we only support ranges for last designator */
    while (nb_elems == 1 && (tok == '[' || tok == '.')) {
        if (tok == '[') {
            if (!(type->t & VT_ARRAY))
                expect("array type");
            next();
            index = index_last = expr_const();
            if (tok == TOK_DOTS && gnu_ext) {
                next();
                index_last = expr_const();
            }
            skip(']');
            s = type->ref;
            decl_design_flex(p, s, index_last);
            if (index < 0 || index_last >= s->c || index_last < index)
	        tcc_error("index exceeds array bounds or range is empty");
            if (cur_field)
		(*cur_field)->c = index_last;
            type = pointed_type(type);
            elem_size = type_size(type, &align);
            c += index * elem_size;
            nb_elems = index_last - index + 1;
        } else {
            int cumofs;
            next();
            l = tok;
        struct_field:
            next();
	    f = find_field(type, l, &cumofs);
            if (cur_field)
                *cur_field = f;
	    type = &f->type;
            c += cumofs;
        }
        cur_field = NULL;
    }
    if (!cur_field) {
        if (tok == '=') {
            next();
        } else if (!gnu_ext) {
	    expect("=");
        }
    } else {
    no_designator:
        if (type->t & VT_ARRAY) {
	    index = (*cur_field)->c;
            s = type->ref;
            decl_design_flex(p, s, index);
            if (index >= s->c)
                tcc_error("too many initializers");
            type = pointed_type(type);
            elem_size = type_size(type, &align);
            c += index * elem_size;
        } else {
            f = *cur_field;
	    /* Skip bitfield padding. Also with size 32 and 64. */
	    while (f && (f->v & SYM_FIRST_ANOM) &&
		   is_integer_btype(f->type.t & VT_BTYPE))
	        *cur_field = f = f->next;
            if (!f)
                tcc_error("too many initializers");
	    type = &f->type;
            c += f->c;
        }
    }

    if (!elem_size) /* for structs */
        elem_size = type_size(type, &align);

    /* Using designators the same element can be initialized more
       than once.  In that case we need to delete possibly already
       existing relocations. */
    if (!(flags & DIF_SIZE_ONLY) && c - corig < al) {
        decl_design_delrels(p->sec, c, elem_size * nb_elems);
        flags &= ~DIF_CLEAR; /* mark stack dirty too */
    }

    decl_initializer(p, type, c, flags & ~DIF_FIRST);

    if (!(flags & DIF_SIZE_ONLY) && nb_elems > 1) {
        Sym aref = {0};
        CType t1;
        int i;
        if (p->sec || (type->t & VT_ARRAY)) {
            /* make init_putv/vstore believe it were a struct */
            aref.c = elem_size;
            t1.t = VT_STRUCT, t1.ref = &aref;
            type = &t1;
        }
        if (p->sec)
            vpush_ref(type, p->sec, c, elem_size);
        else
	    vset(type, VT_LOCAL|VT_LVAL, c);
        for (i = 1; i < nb_elems; i++) {
            vdup();
            init_putv(p, type, c + elem_size * i);
	}
        vpop();
    }

    c += nb_elems * elem_size;
    if (c - corig > al)
      al = c - corig;
    return al;
}

static void write_ldouble(unsigned char *d, void *s)
{
    //printf("long double %Lf\n", *(long double*)s);
#ifdef TCC_CROSS_TEST
    if (LDOUBLE_SIZE >= 10) {
        double b = *(long double*)s;
        s = &b;
#else
    if (sizeof (long double) == 8 && LDOUBLE_SIZE >= 10) {
#endif
        /* our 'long double' is a double really (_WIN32, __APPLE__) */
        uint64_t m = *(uint64_t*)s;
        int e = m >> 48;
        int f = e >> 4 & 0x7FF;
        m <<= 11;
        if (0 == f) {
            if (0 == m)
                goto set;
            for (f = 1; !(m & 1ULL<<63); --f)
                m <<= 1;
        }
        if (f == 0x7ff)
            f = 0x43FF;
        e = (e & 0x8000) | (f + 0x3C00);
        m |= 1ULL<<63;
    set:
    #if (defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64)
        /* double -> extended */
        write64le(d, m);
        write16le(d+8, e);
    #elif LDOUBLE_SIZE == 16
        /* double -> quad */
        write64le(d+6, m << 1);
        write16le(d+14, e);
    #endif
        ;
    } else {
    #if LDOUBLE_SIZE == 8
        /* long double -> double */
        double b = *(long double*)s;
        memcpy(d, &b, 8);
    #elif (__i386__ || __x86_64__) && (defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64)
        /* extended -> extended */
        memcpy(d, s, 10);
    #elif (__i386__ || __x86_64__) && (defined TCC_TARGET_ARM64 || defined TCC_TARGET_RISCV64)
        /* extended -> quad */
        uint64_t m = *(uint64_t*)s;
        int e = *(uint16_t*)((char*)s + 8);
        write64le(d+6, m << 1);
        write16le(d+14, e);
    #elif (__aarch64__ || __riscv) && (defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64)
        /* quad -> extended */
        uint64_t m = read64le((char*)s + 6);
        int e = read16le((char*)s + 14);
        (e & 0x7fff) && (m & 1) && 0 == ++m && ++e;
        write64le(d, m >> 1 | ((e & 0x7fff) ? 1ULL<<63 : 0));
        write16le(d+8, e);
    #else
        /* unknown */
        if (sizeof (long double) == LDOUBLE_SIZE)
            memcpy(d, s, LDOUBLE_SIZE);
    #endif
    }
}

/* store a value or an expression directly in global data or in local array */
static void init_putv(init_params *p, CType *type, unsigned long c)
{
    int bt;
    void *ptr;
    CType dtype;
    int size, align;
    Section *sec = p->sec;
    uint64_t val;

    dtype = *type;
    dtype.t &= ~VT_CONSTANT; /* need to do that to avoid false warning */

    size = type_size(type, &align);
    if (type->t & VT_BITFIELD)
        size = (BIT_POS(type->t) + BIT_SIZE(type->t) + 7) / 8;
    init_assert(p, c + size);

    if (sec) {
        /* XXX: not portable */
        /* XXX: generate error if incorrect relocation */
        gen_assign_cast(&dtype);
        bt = type->t & VT_BTYPE;

        if ((vtop->r & VT_SYM)
            && bt != VT_PTR
            && (bt != (PTR_SIZE == 8 ? VT_LLONG : VT_INT)
                || (type->t & VT_BITFIELD))
            && !((vtop->r & VT_CONST) && vtop->sym->v >= SYM_FIRST_ANOM)
            )
            tcc_error("initializer element is not computable at load time");

        if (NODATA_WANTED) {
            vtop--;
            return;
        }

        ptr = sec->data + c;
        val = vtop->c.i;

	if ((vtop->r & (VT_SYM|VT_CONST)) == (VT_SYM|VT_CONST)
            && vtop->sym->v >= SYM_FIRST_ANOM
            && ((vtop->r & VT_LVAL) /* compound literal */
                || bt == VT_STRUCT /* designator */
                )) {
	    /* memcpy stuff over.  */
	    Section *ssec;
	    ElfSym *esym;
	    ElfW_Rel *rel;
	    esym = elfsym(vtop->sym);
	    ssec = tcc_state->sections[esym->st_shndx];
	    memmove (ptr, ssec->data + esym->st_value + (int)vtop->c.i, size);
	    if (ssec->reloc) {
		/* We need to copy over all memory contents, and that
		   includes relocations.  Use the fact that relocs are
		   created it order, so look from the end of relocs
		   until we hit one before the copied region.  */
                unsigned long relofs = ssec->reloc->data_offset;
		while (relofs >= sizeof(*rel)) {
                    relofs -= sizeof(*rel);
                    rel = (ElfW_Rel*)(ssec->reloc->data + relofs);
		    if (rel->r_offset >= esym->st_value + size)
		      continue;
		    if (rel->r_offset < esym->st_value)
		      break;
		    put_elf_reloca(symtab_section, sec,
				   c + rel->r_offset - esym->st_value,
				   ELFW(R_TYPE)(rel->r_info),
				   ELFW(R_SYM)(rel->r_info),
#if PTR_SIZE == 8
				   rel->r_addend
#else
				   0
#endif
				  );
		}
	    }
	} else {
            if (type->t & VT_BITFIELD) {
                int bit_pos, bit_size, bits, n;
                unsigned char *p, v, m;
                bit_pos = BIT_POS(vtop->type.t);
                bit_size = BIT_SIZE(vtop->type.t);
                p = (unsigned char*)ptr + (bit_pos >> 3);
                bit_pos &= 7, bits = 0;
                while (bit_size) {
                    n = 8 - bit_pos;
                    if (n > bit_size)
                        n = bit_size;
                    v = val >> bits << bit_pos;
                    m = ((1 << n) - 1) << bit_pos;
                    *p = (*p & ~m) | (v & m);
                    bits += n, bit_size -= n, bit_pos = 0, ++p;
                }
            } else
            switch(bt) {
	    case VT_BOOL:
		*(char *)ptr = val != 0;
                break;
	    case VT_BYTE:
		*(char *)ptr = val;
		break;
	    case VT_SHORT:
                write16le(ptr, val);
		break;
	    case VT_FLOAT:
                write32le(ptr, val);
		break;
	    case VT_DOUBLE:
                write64le(ptr, val);
		break;
	    case VT_LDOUBLE:
                write_ldouble(ptr, &vtop->c.ld);
		break;

#if PTR_SIZE == 8
            /* intptr_t may need a reloc too, see tcctest.c:relocation_test() */
	    case VT_LLONG:
	    case VT_PTR:
	        if (vtop->r & VT_SYM)
	          greloca(sec, vtop->sym, c, R_DATA_PTR, val);
	        else
	          write64le(ptr, val);
	        break;
            case VT_INT:
                write32le(ptr, val);
                break;
#else
	    case VT_LLONG:
                write64le(ptr, val);
                break;
            case VT_PTR:
            case VT_INT:
	        if (vtop->r & VT_SYM)
	          greloc(sec, vtop->sym, c, R_DATA_PTR);
	        write32le(ptr, val);
	        break;
#endif
	    default:
                //tcc_internal_error("unexpected type");
                break;
	    }
	}
        vtop--;
    } else {
        vset(&dtype, VT_LOCAL|VT_LVAL, c);
        vswap();
        vstore();
        vpop();
    }
}

/* 't' contains the type and storage info. 'c' is the offset of the
   object in section 'sec'. If 'sec' is NULL, it means stack based
   allocation. 'flags & DIF_FIRST' is true if array '{' must be read (multi
   dimension implicit array init handling). 'flags & DIF_SIZE_ONLY' is true if
   size only evaluation is wanted (only for arrays). */
static void decl_initializer(init_params *p, CType *type, unsigned long c, int flags)
{
    int len, n, no_oblock, i;
    int size1, align1;
    Sym *s, *f;
    Sym indexsym;
    CType *t1;

    /* generate line number info */
    if (debug_modes && !(flags & DIF_SIZE_ONLY) && !p->sec)
        tcc_debug_line(tcc_state), tcc_tcov_check_line (tcc_state, 1);

    if (!(flags & DIF_HAVE_ELEM) && tok != '{' &&
	/* In case of strings we have special handling for arrays, so
	   don't consume them as initializer value (which would commit them
	   to some anonymous symbol).  */
	tok != TOK_LSTR && tok != TOK_STR &&
	(!(flags & DIF_SIZE_ONLY)
            /* a struct may be initialized from a struct of same type, as in
                    struct {int x,y;} a = {1,2}, b = {3,4}, c[] = {a,b};
               In that case we need to parse the element in order to check
               it for compatibility below */
            || (type->t & VT_BTYPE) == VT_STRUCT)
        ) {
        int ncw_prev = nocode_wanted;
        if ((flags & DIF_SIZE_ONLY) && !p->sec)
            ++nocode_wanted;
	parse_init_elem(!p->sec ? EXPR_ANY : EXPR_CONST);
        nocode_wanted = ncw_prev;
        flags |= DIF_HAVE_ELEM;
    }

    if (type->t & VT_ARRAY) {
        no_oblock = 1;
        if (((flags & DIF_FIRST) && tok != TOK_LSTR && tok != TOK_STR) ||
            tok == '{') {
            skip('{');
            no_oblock = 0;
        }

        s = type->ref;
        n = s->c;
        t1 = pointed_type(type);
        size1 = type_size(t1, &align1);

        /* only parse strings here if correct type (otherwise: handle
           them as ((w)char *) expressions */
        if ((tok == TOK_LSTR && (t1->t & VT_BTYPE) == VT_INT) /* tcc_posix: wchar_t 统一 4 字节 */
            || (tok == TOK_STR && (t1->t & VT_BTYPE) == VT_BYTE)) {
	    len = 0;
            cstr_reset(&initstr);
            if (size1 != (tok == TOK_STR ? 1 : sizeof(nwchar_t)))
              tcc_error("unhandled string literal merging");
            while (tok == TOK_STR || tok == TOK_LSTR) {
                if (initstr.size)
                  initstr.size -= size1;
                if (tok == TOK_STR)
                  len += tokc.str.size;
                else
                  len += tokc.str.size / sizeof(nwchar_t);
                len--;
                cstr_cat(&initstr, tokc.str.data, tokc.str.size);
                next();
            }
            if (tok != ')' && tok != '}' && tok != ',' && tok != ';'
                && tok != TOK_EOF) {
                /* Not a lone literal but part of a bigger expression.  */
                unget_tok(size1 == 1 ? TOK_STR : TOK_LSTR);
                tokc.str.size = initstr.size;
                tokc.str.data = initstr.data;
                goto do_init_array;
            }

            decl_design_flex(p, s, len);
            if (!(flags & DIF_SIZE_ONLY)) {
                int nb = n, ch;
                if (len < nb)
                    nb = len;
                if (len > nb)
                  tcc_warning("initializer-string for array is too long");
                /* in order to go faster for common case (char
                   string in global variable, we handle it
                   specifically */
                if (p->sec && size1 == 1) {
                    init_assert(p, c + nb);
                    if (!NODATA_WANTED)
                      memcpy(p->sec->data + c, initstr.data, nb);
                } else {
                    for(i=0;i<n;i++) {
                        if (i >= nb) {
                          /* only add trailing zero if enough storage (no
                             warning in this case since it is standard) */
                          if (flags & DIF_CLEAR)
                            break;
                          if (n - i >= 4) {
                            init_putz(p, c + i * size1, (n - i) * size1);
                            break;
                          }
                          ch = 0;
                        } else if (size1 == 1)
                          ch = ((unsigned char *)initstr.data)[i];
                        else
                          ch = ((nwchar_t *)initstr.data)[i];
                        vpushi(ch);
                        init_putv(p, t1, c + i * size1);
                    }
                }
            }
            if (tok == ',' && !no_oblock) /* static const char s[] = { "123", }; */
                next();
        } else {

          do_init_array:
	    indexsym.c = 0;
	    f = &indexsym;

          do_init_list:
            /* zero memory once in advance */
            if (!(flags & (DIF_CLEAR | DIF_SIZE_ONLY))) {
                init_putz(p, c, n*size1);
                flags |= DIF_CLEAR;
            }

	    len = 0;
            /* GNU extension: if the initializer is empty for a flex array,
               it's size is zero.  We won't enter the loop, so set the size
               now.  */
            decl_design_flex(p, s, len - 1);
	    while (tok != '}' || (flags & DIF_HAVE_ELEM)) {
		len = decl_designator(p, type, c, &f, flags, len);
		flags &= ~DIF_HAVE_ELEM;
		if (type->t & VT_ARRAY) {
		    ++indexsym.c;
		    /* special test for multi dimensional arrays (may not
		       be strictly correct if designators are used at the
		       same time) */
		    if (no_oblock && len >= n*size1)
		        break;
		} else {
		    if (s->type.t == VT_UNION)
		        f = NULL;
		    else
		        f = f->next;
		    if (no_oblock && f == NULL)
		        break;
		}

		if (tok == '}')
		    break;
		skip(',');
	    }
        }
        if (!no_oblock)
            skip('}');

    } else if ((flags & DIF_HAVE_ELEM)
        /* Use i_c_parameter_t, to strip toplevel qualifiers.
           The source type might have VT_CONSTANT set, which is
           of course assignable to non-const elements.  */
            && is_compatible_unqualified_types(type, &vtop->type)) {
        goto one_elem;

    } else if ((type->t & VT_BTYPE) == VT_STRUCT) {
        no_oblock = 1;
        if ((flags & DIF_FIRST) || tok == '{') {
            skip('{');
            no_oblock = 0;
        }
        s = type->ref;
        f = s->next;
        n = s->c;
        size1 = 1;
	goto do_init_list;

    } else if (tok == '{') {
        if (flags & DIF_HAVE_ELEM)
          skip(';');
        next();
        decl_initializer(p, type, c, flags & ~DIF_HAVE_ELEM);
        skip('}');

    } else one_elem: if ((flags & DIF_SIZE_ONLY)) {
	/* If we supported only ISO C we wouldn't have to accept calling
	   this on anything than an array if DIF_SIZE_ONLY (and even then
	   only on the outermost level, so no recursion would be needed),
	   because initializing a flex array member isn't supported.
	   But GNU C supports it, so we need to recurse even into
	   subfields of structs and arrays when DIF_SIZE_ONLY is set.  */
        /* just skip expression */
        if (flags & DIF_HAVE_ELEM)
            vpop();
        else
            skip_or_save_block(NULL);

    } else {
	if (!(flags & DIF_HAVE_ELEM)) {
	    /* This should happen only when we haven't parsed
	       the init element above for fear of committing a
	       string constant to memory too early.  */
	    if (tok != TOK_STR && tok != TOK_LSTR)
	      expect("string constant");
	    parse_init_elem(!p->sec ? EXPR_ANY : EXPR_CONST);
	}
        if (!p->sec && (flags & DIF_CLEAR) /* container was already zero'd */
            && (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
            && vtop->c.i == 0
            && btype_size(type->t & VT_BTYPE) /* not for fp constants */
            )
            vpop();
        else
            init_putv(p, type, c);
    }
}

/* parse an initializer for type 't' if 'has_init' is non zero, and
   allocate space in local or global data space ('r' is either
   VT_LOCAL or VT_CONST). If 'v' is non zero, then an associated
   variable 'v' of scope 'scope' is declared before initializers
   are parsed. If 'v' is zero, then a reference to the new object
   is put in the value stack. If 'has_init' is 2, a special parsing
   is done to handle string constants. */
static void decl_initializer_alloc(CType *type, AttributeDef *ad, int r, 
                                   int has_init, int v, int scope)
{
    int size, align, addr;
    TokenString *init_str = NULL;

    Section *sec;
    Sym *flexible_array;
    Sym *sym = NULL;
    int saved_nocode_wanted = nocode_wanted;
    int tls_desc_ofs = -1;  /* data_section offset of the emutls descriptor, or -1 */
#ifdef CONFIG_TCC_BCHECK
    int bcheck = tcc_state->do_bounds_check && !NODATA_WANTED;
#endif
    init_params p = {0};

    if (scope == VT_CONST) {
        /* see if a global symbol was already defined */
        sym = sym_find(v);
        if (sym) {
            patch_storage(sym, ad, type);
            /* we accept several definitions of the same global variable. */
            if (!has_init && sym->c && elfsym(sym)->st_shndx != SHN_UNDEF)
                return;
            type = &sym->type;
        }
    }

    /* Always allocate static or global variables */
    if (v && (r & VT_VALMASK) == VT_CONST)
        nocode_wanted |= DATA_ONLY_WANTED;

    flexible_array = NULL;
    size = type_size(type, &align);

    /* exactly one flexible array may be initialized, either the
       toplevel array or the last member of the toplevel struct */

    if (size < 0) {
        // error out except for top-level incomplete arrays
        // (arrays of incomplete types are handled in array parsing)
        if (!(type->t & VT_ARRAY))
            tcc_error("initialization of incomplete type");
        /* If the base type itself was an array type of unspecified
           size (like in 'typedef int arr[]; arr x = {1};') then
           we will overwrite the unknown size by the real one for
           this decl.  We need to unshare the ref symbol holding
           that size.  */
        if (IS_BT_ARRAY(type->t))
            type->ref = sym_push(SYM_FIELD, &type->ref->type, 0, type->ref->c);
        p.flex_array_ref = type->ref;

    } else if (has_init && (type->t & VT_BTYPE) == VT_STRUCT) {
        Sym *field = type->ref->next;
        if (field) {
            while (field->next)
                field = field->next;
            if (field->type.t & VT_ARRAY && field->type.ref->c < 0) {
                flexible_array = field;
                p.flex_array_ref = field->type.ref;
                size = -1;
            }
        }
    }

    if (size < 0) {
        /* If unknown size, do a dry-run 1st pass */
        if (!has_init)
            goto err_size;
        if (has_init == 2) {
            /* only get strings */
            init_str = tok_str_alloc();
            while (tok == TOK_STR || tok == TOK_LSTR) {
                tok_str_add_tok(init_str);
                next();
            }
            tok_str_add(init_str, TOK_EOF);
        } else
            skip_or_save_block(&init_str);
        unget_tok(0);

        /* compute size */
        begin_macro(init_str, 1);
        next();
        decl_initializer(&p, type, 0, DIF_FIRST | DIF_SIZE_ONLY);
        /* prepare second initializer parsing */
        macro_ptr = init_str->str;
        next();

        /* if still unknown size, error */
        size = type_size(type, &align);
        if (size < 0)
    err_size:
            tcc_error("unknown type size");

        /* If there's a flex member and it was used in the initializer
           adjust size.  */
        if (flexible_array && flexible_array->type.ref->c > 0)
            size += flexible_array->type.ref->c
                    * pointed_size(&flexible_array->type);
    }

    /* take into account specified alignment if bigger */
    if (ad->a.aligned) {
	int speca = 1 << (ad->a.aligned - 1);
        if (speca > align)
            align = speca;
    } else if (ad->a.packed) {
        align = 1;
    }

    if (!v && NODATA_WANTED)
        size = 0, align = 1;

    if ((r & VT_VALMASK) == VT_LOCAL) {
        CType ltype = *type;
        /* local __thread: function-locals are already per-thread on the
           stack, so treat them as a plain automatic variable (no emutls
           descriptor / no interception). */
        ltype.t &= ~VT_TLS;
        type = &ltype;
        sec = NULL;
#ifdef CONFIG_TCC_BCHECK
        if (bcheck && v) {
            /* add padding between stack variables for bound checking */
            loc -= align;
        }
#endif
        loc = (loc - size) & -align;
        addr = loc;
        p.local_offset = addr + size;
#ifdef CONFIG_TCC_BCHECK
        if (bcheck && v) {
            /* add padding between stack variables for bound checking */
            loc -= align;
        }
#endif
        if (v) {
            /* local variable */
#ifdef CONFIG_TCC_ASM
	    if (ad->asm_label) {
		int reg = asm_parse_regvar(ad->asm_label);
		if (reg >= 0)
		    r = (r & ~VT_VALMASK) | reg;
	    }
#endif
            sym = sym_push(v, type, r, addr);
	    if (ad->cleanup_func) {
		Sym *cls = sym_push2(&all_cleanups,
                    SYM_FIELD | ++cur_scope->cl.n, 0, 0);
		cls->cleanup_sym = sym;
		cls->cleanup_func = ad->cleanup_func;
		cls->next = cur_scope->cl.s;
		cur_scope->cl.s = cls;
	    }

            sym->a = ad->a;
        } else {
            /* push local reference */
            vset(type, r, addr);
        }
    } else {
        /* allocate symbol in corresponding section */
        sec = ad->section;
        if (!sec) {
            CType *tp = type;
            while ((tp->t & (VT_BTYPE|VT_ARRAY)) == (VT_PTR|VT_ARRAY))
                tp = &tp->ref->type;
            if (type->t & VT_TLS) {
                /* tcc_posix: __thread -> emutls.  The C symbol gets a dead
                   placeholder section (the real per-thread storage is
                   allocated by the emutls runtime allocator at runtime); we
                   also emit a `struct __emutls_object` descriptor
                   { size; align; offset; defval } for it. */
                sec = bss_section;
                if (has_init)
                    sec = data_section;  /* hold the (dead) initializer safely */
                if (v) {
                    char nbuf[96];
                    Sym *ds;
                    int da;
                    snprintf(nbuf, sizeof nbuf, "__emutls_v_%s",
                             get_tok_str(v, NULL));
                    da = section_add(data_section, 24, 8);
                    write32le(data_section->data + da + 0, size);    /* size   */
                    write32le(data_section->data + da + 4, align);   /* align  */
                    write32le(data_section->data + da + 8, 0);       /* offset
                                                                        patched by
                                                                        runtime   */
                    write32le(data_section->data + da + 12, 0);
                    write32le(data_section->data + da + 16, 0);      /* defval
                                                                        patched below
                                                                        (reloc->init) */
                    write32le(data_section->data + da + 20, 0);
                    ds = sym_push(tok_alloc_const(nbuf), &char_pointer_type,
                                  VT_CONST | VT_SYM, 0);
                    put_extern_sym(ds, data_section, da, 24);
                    tls_desc_ofs = da;
                }
            } else if (tp->t & VT_CONSTANT) {
		sec = rodata_section;
            } else if (has_init) {
		sec = data_section;
                /*if (g_debug & 4)
                    tcc_warning("rw data: %s", get_tok_str(v, 0));*/
            } else if (tcc_state->nocommon)
                sec = bss_section;
        }

        if (sec) {
	    addr = section_add(sec, size, align);
#ifdef CONFIG_TCC_BCHECK
            /* add padding if bound check */
            if (bcheck)
                section_add(sec, 1, 1);
#endif
        } else {
            addr = align; /* SHN_COMMON is special, symbol value is align */
	    sec = common_section;
        }

        if (v) {
            if (!sym) {
                sym = sym_push(v, type, r | VT_SYM, 0);
                patch_storage(sym, ad, NULL);
            }
            /* update symbol definition */
	    put_extern_sym(sym, sec, addr, size);
            if ((type->t & VT_TLS) && tls_desc_ofs >= 0) {
                /* defval: for an initialized __thread, point the descriptor's
                   defval field at the (single-thread) initializer copy, which
                   decl_initializer writes into the placeholder above.  The
                   runtime materializes *defval into per-thread storage on
                   first access.  Uninitialized __thread keeps defval = NULL
                   → zero-init. */
                if (has_init)
                    greloca(data_section, sym, tls_desc_ofs + 16,
                            R_DATA_PTR, 0);
                tls_desc_ofs = -1;
            }
        } else {
            /* push global reference */
            vpush_ref(type, sec, addr, size);
            sym = vtop->sym;
	    vtop->r |= r;
        }

#ifdef CONFIG_TCC_BCHECK
        /* handles bounds now because the symbol must be defined
           before for the relocation */
        if (bcheck) {
            addr_t *bounds_ptr;

            greloca(bounds_section, sym, bounds_section->data_offset, R_DATA_PTR, 0);
            /* then add global bound info */
            bounds_ptr = section_ptr_add(bounds_section, 2 * sizeof(addr_t));
            bounds_ptr[0] = 0; /* relocated */
            bounds_ptr[1] = size;
        }
#endif
    }

    if (type->t & VT_VLA) {
        int a;

        if (has_init)
            tcc_error("variable length array cannot be initialized");

        if (NODATA_WANTED)
            goto no_alloc;

        /* save before-VLA stack pointer if needed */
        if (cur_scope->vla.num == 0) {
            if (cur_scope->prev && cur_scope->prev->vla.num) {
                cur_scope->vla.locorig = cur_scope->prev->vla.loc;
            } else {
                gen_vla_sp_save(loc -= PTR_SIZE);
                cur_scope->vla.locorig = loc;
            }
        }

        vpush_type_size(type, &a);
        gen_vla_alloc(type, a);
#if defined TCC_TARGET_PE && defined TCC_TARGET_X86_64
        /* on _WIN64, because of the function args scratch area, the
           result of alloca differs from RSP and is returned in RAX.  */
        gen_vla_result(addr), addr = (loc -= PTR_SIZE);
#endif
        gen_vla_sp_save(addr);
        cur_scope->vla.loc = addr;
        cur_scope->vla.num++;
    } else if (has_init) {
        p.sec = sec;
        decl_initializer(&p, type, addr, DIF_FIRST);
        /* patch flexible array member size back to -1, */
        /* for possible subsequent similar declarations */
        if (flexible_array)
            flexible_array->type.ref->c = -1;
    }

 no_alloc:
    /* restore parse state if needed */
    if (init_str) {
        end_macro();
        next();
    }

    nocode_wanted = saved_nocode_wanted;
}

/* generate vla code saved in post_type() */
static void func_vla_arg_code(Sym *arg)
{
    int align;
    TokenString *vla_array_tok = NULL;

    if (arg->type.ref)
        func_vla_arg_code(arg->type.ref);

    if ((arg->type.t & VT_VLA) && arg->type.ref->vla_array_str) {
	loc -= type_size(&int_type, &align);
	loc &= -align;
	arg->type.ref->c = loc;

	unget_tok(0);
	vla_array_tok = tok_str_alloc();
	vla_array_tok->str = arg->type.ref->vla_array_str;
	begin_macro(vla_array_tok, 1);
	next();
	gexpr();
	end_macro();
	next();
	vpush_type_size(&arg->type.ref->type, &align);
	gen_op('*');
	vset(&int_type, VT_LOCAL|VT_LVAL, arg->type.ref->c);
	vswap();
	vstore();
	vpop();
    }
}

static void func_vla_arg(Sym *sym)
{
    Sym *arg;

    for (arg = sym->type.ref->next; arg; arg = arg->next)
        if ((arg->type.t & VT_BTYPE) == VT_PTR && (arg->type.ref->type.t & VT_VLA))
            func_vla_arg_code(arg->type.ref);
}

/* set the local stack address for function parameter from gfunc_prolog() */
ST_FUNC Sym *gfunc_set_param(Sym *s, int c, int byref)
{
    s = sym_find(s->v);
    if (!s) /* unnamed parameters, not enabled */
        return NULL;
    s->c = c;
    if (byref)
        s->r = VT_LLOCAL | VT_LVAL; /* otherwise VT_LOCAL */
    return s;
}

/* push parameters (and their types), last first */
static void sym_push_params(Sym *ref)
{
    Sym *s = ref;
    while (s->next)
        s = s->next;
    while (s != ref) {
        if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
            sym_copy(s, &local_stack);
        s = s->prev;
    }
}

/* parse a function defined by symbol 'sym' and generate its code in
   'cur_text_section' */
static void gen_function(Sym *sym)
{
    struct scope f = { 0 };

    cur_scope = root_scope = &f;
    nocode_wanted = 0;

    ind = cur_text_section->data_offset;
    if (sym->a.aligned) {
	size_t newoff = section_add(cur_text_section, 0,
				    1 << (sym->a.aligned - 1));
	gen_fill_nops(newoff - ind);
    }

    funcname = get_tok_str(sym->v, NULL);
    func_ind = ind;
    func_vt = sym->type.ref->type;
    func_var = sym->type.ref->f.func_type == FUNC_ELLIPSIS;
    func_old = sym->type.ref->f.func_type == FUNC_OLD;
    nb_defer_descs = 0;   /* defer 记录区每函数重置 */

    /* NOTE: we patch the symbol size later */
    put_extern_sym(sym, cur_text_section, ind, 0);

    if (sym->type.ref->f.func_ctor)
        add_array (tcc_state, ".init_array", sym->c);
    if (sym->type.ref->f.func_dtor)
        add_array (tcc_state, ".fini_array", sym->c);

    /* put debug symbol */
    tcc_debug_funcstart(tcc_state, sym);

    /* push a dummy symbol to enable local sym storage */
    sym_push2(&local_stack, SYM_FIELD, 0, 0);
    /* push parameters */
    local_scope = 1;
    sym_push_params(sym->type.ref);

    local_scope = 0;
    rsym = 0;
    nb_temp_local_vars = 0;

    gfunc_prolog(sym);
    tcc_debug_prolog_epilog(tcc_state, 0);
    func_vla_arg(sym);
    block(0);
    gsym(rsym);
    nocode_wanted = 0;
    tcc_debug_end_scope(NULL, !func_var);
    tcc_debug_prolog_epilog(tcc_state, 1);
    gfunc_epilog();

    /* end of function */
    tcc_debug_funcend(tcc_state, ind - func_ind);

    /* patch symbol size */
    elfsym(sym)->st_size = ind - func_ind;
    cur_text_section->data_offset = ind;

    sym_pop(&local_stack, NULL, 0);
    label_pop(&global_label_stack, NULL, 0);
    sym_pop(&all_cleanups, NULL, 0);
    local_scope = 0;

    /* It's better to crash than to generate wrong code */
    cur_text_section = NULL;
    funcname = ""; /* for safety */
    func_vt.t = VT_VOID; /* for safety */
    func_var = 0; /* for safety */
    ind = 0; /* for safety */
    func_ind = -1;
    nocode_wanted = DATA_ONLY_WANTED;
    check_vstack();

    /* do this after funcend debug info */
    next();
}

static void gen_inline_functions(TCCState *s)
{
    Sym *sym;
    int inline_generated, i;
    struct InlineFunc *fn;

    tcc_open_bf(s, ":inline:", 0);
    /* iterate while inline function are referenced */
    do {
        inline_generated = 0;
        for (i = 0; i < s->nb_inline_fns; ++i) {
            fn = s->inline_fns[i];
            sym = fn->sym;
            if (sym && (sym->c || !(sym->type.t & VT_INLINE))) {
                /* the function was used or forced (and then not internal):
                   generate its code and convert it to a normal function */
                fn->sym = NULL;
                tccpp_putfile(fn->filename);
                begin_macro(fn->func_str, 1);
                next();
                cur_text_section = text_section;
                gen_function(sym);
                end_macro();

                inline_generated = 1;
            }
        }
    } while (inline_generated);
    tcc_close();
}

static void free_inline_functions(TCCState *s)
{
    int i;
    /* free tokens of unused inline functions */
    for (i = 0; i < s->nb_inline_fns; ++i) {
        struct InlineFunc *fn = s->inline_fns[i];
        if (fn->sym)
            tok_str_free(fn->func_str);
    }
    dynarray_reset(&s->inline_fns, &s->nb_inline_fns);
}

static void do_Static_assert(void)
{
    int c;
    const char *msg;

    next();
    skip('(');
    c = expr_const();
    msg = "_Static_assert fail";
    if (tok == ',') {
        next();
        msg = parse_mult_str("string constant")->data;
    }
    skip(')');
    if (c == 0)
        tcc_error("%s", msg);
    skip(';');
}

#ifdef TCC_TARGET_PE
static void pe_check_linkage(CType *type, AttributeDef *ad)
{
    if (!ad->a.dllimport && !ad->a.dllexport)
        return;
    if (type->t & VT_STATIC)
        tcc_error("cannot have dll linkage with static");
    if (type->t & VT_TYPEDEF) {
        const char *m = ad->a.dllimport ? "im" : "ex";
        tcc_warning("'dll%sport' attribute ignored for typedef", m);
        ad->a.dllimport = 0;
        ad->a.dllexport = 0;
    } else if (ad->a.dllimport) {
        if ((type->t & VT_BTYPE) == VT_FUNC)
            ad->a.dllimport = 0;
        else
            type->t |= VT_EXTERN;
    }
}
#endif

/* 'l' is VT_LOCAL or VT_CONST to define default storage type
   or VT_CMP if parsing old style parameter list
   or VT_JMP if parsing c99 for decl: for (int i = 0, ...) */
static int decl(int l)
{
    int v, has_init, r, oldint;
    CType type, btype;
    Sym *sym, *sa;
    AttributeDef ad, adbase;
    ElfSym *esym;

    while (1) {

        oldint = 0;
        if (!parse_btype(&btype, &adbase, l == VT_LOCAL)) {
            if (model_decl_seen) {
                /* model 定义已由 parse_btype 消费, 直接继续 */
                model_decl_seen = 0;
                continue;
            }
            if (l == VT_JMP)
                return 0;
            /* skip redundant ';' if not in old parameter decl scope */
            if (tok == ';' && l != VT_CMP) {
                next();
                continue;
            }
            if (tok == TOK_STATIC_ASSERT) {
                do_Static_assert();
                continue;
            }
            if (l != VT_CONST)
                break;
            if (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3) {
                /* global asm block */
                asm_global_instr();
                continue;
            }
            if (tok >= TOK_UIDENT) {
               /* special test for old K&R protos without explicit int
                  type. Only accepted when defining global data */
                btype.t = VT_INT;
                oldint = 1;
            } else {
                if (tok != TOK_EOF)
                    expect("declaration");
                break;
            }
        }

        if (tok == ';') {
            if ((btype.t & VT_BTYPE) == VT_STRUCT
                && (btype.ref->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
                ; /* struct decl with named tag */
            else if (IS_ENUM(btype.t))
                ; /* enum decl */
            else
                tcc_warning("useless type defines no instances");
            if (l == VT_JMP)
                return 1;
            next();
            continue;
        }

        while (1) { /* iterate thru each declaration */
            type = btype;
	    ad = adbase;
            type_decl(&type, &ad, &v, l == VT_CMP ? TYPE_DIRECT | TYPE_PARAM : TYPE_DIRECT);
            /*ptype("decl", &type, v);*/
            if ((type.t & VT_BTYPE) == VT_FUNC) {
                if ((type.t & VT_STATIC) && (l != VT_CONST))
                    tcc_error("function without file scope cannot be static");
                /* if old style function prototype, we accept a
                   declaration list */
                sym = type.ref;
                if (sym->f.func_type == FUNC_OLD && l == VT_CONST) {
                    func_vt = type;
		    ++local_scope;
                    decl(VT_CMP);
		    --local_scope;
                }
                if ((type.t & (VT_EXTERN|VT_INLINE)) == (VT_EXTERN|VT_INLINE)) {
                    /* always_inline functions must be handled as if they
                       don't generate multiple global defs, even if extern
                       inline, i.e. GNU inline semantics for those.  Rewrite
                       them into static inline.  */
                    if (tcc_state->gnu89_inline || sym->f.func_alwinl)
                        type.t = (type.t & ~VT_EXTERN) | VT_STATIC;
                    else
                        type.t &= ~VT_INLINE; /* always compile otherwise */
                }

            } else if (oldint) {
                tcc_warning("type defaults to int");
            }

            if (gnu_ext && (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3)) {
                ad.asm_label = asm_label_instr();
                /* parse one last attribute list, after asm label */
                parse_attribute(&ad);
            #if 0
                /* gcc does not allow __asm__("label") with function definition,
                   but why not ... */
                if (tok == '{')
                    expect(";");
            #endif
            }

#ifdef TCC_TARGET_PE
            pe_check_linkage(&type, &ad);
#endif
            if (tok == '{') {
                if (l != VT_CONST)
                    tcc_error("cannot use local functions");
                if ((type.t & VT_BTYPE) != VT_FUNC)
                    expect("function definition");

                /* apply post-declaraton attributes */
                merge_funcattr(&type.ref->f, &ad.f);
                /* put function symbol */
                type.t &= ~VT_EXTERN;
                sym = external_sym(v, &type, 0, &ad);

                /* reject abstract declarators in function definition
                   make old-style float params double */
                for (sa = sym->type.ref; (sa = sa->next) != NULL;) {
                    if (!(sa->v & ~SYM_FIELD))
                        expect("identifier");
                    if (sa->type.t == VT_FLOAT
                        && sym->type.ref->f.func_type == FUNC_OLD) {
                        sa->type.t = VT_DOUBLE;
                    }
                }

                /* static inline functions are just recorded as a kind
                   of macro. Their code will be emitted at the end of
                   the compilation unit only if they are used */
                if (sym->type.t & VT_INLINE) {
                    struct InlineFunc *fn;
                    fn = tcc_malloc(sizeof *fn + strlen(file->filename));
                    strcpy(fn->filename, file->filename);
                    fn->sym = sym;
                    dynarray_add(&tcc_state->inline_fns,
				 &tcc_state->nb_inline_fns, fn);
                    skip_or_save_block(&fn->func_str);
                } else {
                    /* compute text section */
                    cur_text_section = ad.section;
                    if (!cur_text_section)
                        cur_text_section = text_section;
                    else if (cur_text_section->sh_num > bss_section->sh_num)
                        cur_text_section->sh_flags = text_section->sh_flags;
                    gen_function(sym);
                }
                break;
            } else {
                has_init = 0;
		if (l == VT_CMP) {
		    /* find parameter in function parameter list */
		    for (sym = func_vt.ref->next; sym; sym = sym->next)
			if ((sym->v & ~SYM_FIELD) == v)
			    goto found;
		    tcc_error("declaration for parameter '%s' but no such parameter",
			      get_tok_str(v, NULL));
                found:
		    if (type.t & VT_STORAGE) /* 'register' is okay */
		        tcc_error("storage class specified for '%s'",
				  get_tok_str(v, NULL));
		    if (!(sym->type.t & VT_EXTERN))
		        tcc_error("redefinition of parameter '%s'",
				  get_tok_str(v, NULL));
		    convert_parameter_type(&type);
		    sym->type = type;
		} else if (type.t & VT_TYPEDEF) {
                    /* save typedefed type  */
                    /* XXX: test storage specifiers ? */
                    sym = sym_find(v);
                    if (sym && sym->sym_scope == local_scope) {
                        if (!is_compatible_types(&sym->type, &type)
                            || !(sym->type.t & VT_TYPEDEF))
                            tcc_error("incompatible redefinition of '%s'",
                                get_tok_str(v, NULL));
                        sym->type = type;
                    } else {
                        sym = sym_push(v, &type, 0, 0);
                    }
                    sym->a = ad.a;
                    if ((type.t & VT_BTYPE) == VT_FUNC)
                      merge_funcattr(&sym->type.ref->f, &ad.f);
                    if (debug_modes)
                        tcc_debug_typedef (tcc_state, sym);
		} else if ((type.t & VT_BTYPE) == VT_VOID
			   && !(type.t & VT_EXTERN)) {
		    tcc_error("declaration of void object");
                } else {
                    r = 0;
                    if ((type.t & VT_BTYPE) == VT_FUNC) {
                        /* external function definition */
                        /* specific case for func_call attribute */
                        merge_funcattr(&type.ref->f, &ad.f);
                    } else if (!(type.t & VT_ARRAY)) {
                        /* not lvalue if array */
                        r |= VT_LVAL;
                    }

                    if (tok == '=')
                        has_init = 1;

                    if (((type.t & VT_EXTERN) && (!has_init || l != VT_CONST))
		        || (type.t & VT_BTYPE) == VT_FUNC
                        /* as with GCC, uninitialized global arrays with no size
                           are considered extern: */
                        || ((type.t & VT_ARRAY) && !has_init
                            && l == VT_CONST && type.ref->c < 0)
                        ) {
                        /* external variable or function */
                        type.t |= VT_EXTERN;
                        external_sym(v, &type, r, &ad);
                    } else {
                        if (l == VT_CONST || (type.t & VT_STATIC))
                            r |= VT_CONST;
                        else
                            r |= VT_LOCAL;
                        type.t &= ~VT_EXTERN;
                        if (has_init)
                            next();
                        else if (l == VT_CONST)
                            /* uninitialized global variables may be overridden */
                            type.t |= VT_EXTERN;
                        decl_initializer_alloc(&type, &ad, r, has_init, v, l);
                    }

                    if (ad.alias_target && l == VT_CONST) {
                        /* Aliases need to be emitted when their target symbol
                           is emitted, even if perhaps unreferenced.
                           We only support the case where the base is already
                           defined, otherwise we would need deferring to emit
                           the aliases until the end of the compile unit.  */
                        esym = elfsym(sym_find(ad.alias_target));
                        if (!esym)
                            tcc_error("unsupported forward __alias__ attribute");
                        put_extern_sym2(sym_find(v), esym->st_shndx,
                                        esym->st_value, esym->st_size, 1);
                    }
                }
                if (tok != ',') {
                    if (l == VT_JMP)
                        return has_init ? v : 1;
                    skip(';');
                    break;
                }
                next();
            }
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
#undef gjmp_addr
#undef gjmp
/* ------------------------------------------------------------------------- */
