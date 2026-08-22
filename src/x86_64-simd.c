/*
 *  x86-64 SSE SIMD 后端 (独立模块)
 *
 *  从 tccgen.c 拆出. 128-bit 向量永远存于 16 字节对齐的内存槽(临时局部变量),
 *  不引入寄存器分配器/XMM 分配改动. 前端只留两个通用钩子:
 *    - unary() 调用 simd_builtin_dispatch(tok) 处理 _mm_* 内建函数
 *    - gen_op() 调用 simd_gen_op(op) 处理 v4f+v4f 等原生运算符
 *  指令发射复用 x86_64-gen.c 的 gen_v128*, 经 tcc.h 前置原型跨模块调用.
 *
 *  kind: SIMD_F4(float*4, movaps/addps...), SIMD_D2(double*2, movapd/addpd...),
 *        SIMD_I4(int32*4, movdqa/paddd/psubd/pmulld + 标量 idiv),
 *        SIMD_W16(int16*8, paddw/psubw/pmullw + 标量 idiv),
 *        SIMD_B8(int8*16, paddb/psubb + 标量兜底).
 */

#ifdef TCC_TARGET_X86_64

/* 与其他 x86_64-*.c 后端一致: tcc.h 的 state 宏经 USING_GLOBALS 走
   tcc_state-> (而非 s1->), 使 tcc_error/tcc_warning 等宏在无局部 s1 时可用. */
#define USING_GLOBALS
#include "tcc.h"

enum { SIMD_F4 = 0, SIMD_D2, SIMD_I4, SIMD_W16, SIMD_B8 };
#define NB_SIMD_KIND  5

/* 每个 kind 的参数: vtype 名, 是否整型, 元素字节, add/sub/mul opcode (mul 0=无打包) */
typedef struct {
    const char *name;         /* <simd.h> 类型名 */
    int is_int;               /* 整型: 需 66 前缀 + 标量除法 */
    int esize;                /* 元素字节: 4/2/1 */
    int add_op, sub_op, mul_op;
    const char *nm3;          /* tok_alloc 长度提示 (3 字母) */
} SimdKindInfo;

static const SimdKindInfo simd_kind[NB_SIMD_KIND] = {
    { "v4f",  0, 4, 0x58, 0x5c, 0x59, "v4f" },  /* F4: 无前缀 */
    { "v2d",  0, 8, 0x58, 0x5c, 0x59, "v2d" },  /* D2: 66 前缀, 与 ps 同 opcode */
    { "v4i",  1, 4, 0xfe, 0xfa, 0x40, "v4i" },  /* I4: pmulld(SSE4.1) */
    { "v8h",  1, 2, 0xfd, 0xf9, 0xd5, "v8h" },  /* W16: pmullw(SSE2) */
    { "v16b", 1, 1, 0xfc, 0xf8, 0,    "v16b" }, /* B8: 无打包乘法 -> 标量 */
};

static int simd_is_int(int kind)
{
    return simd_kind[kind].is_int;
}

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

/* 判定 vstack 顶的类型是否为 SIMD 向量; 是则返回 kind, 否则 -1. */
ST_FUNC int simd_vector_kind(CType *t)
{
    int k;
    Sym *s = t->ref;
    if ((t->t & VT_BTYPE) != VT_STRUCT || !s)
        return -1;
    /* 向量类型是 <simd.h> 里 typedef struct 的匿名 tag 符号: 其 token(s->v)
       由 TCC 自动合成, 与 typedef 名不同, 故用指针比较与 simd_vtype(k) 解析
       出的同一结构体符号对比来识别 kind. */
    for (k = 0; k < NB_SIMD_KIND; k++) {
        CType vt = simd_vtype(k);
        if (vt.ref == s)
            return k;
    }
    return -1;
}

/* 把已填充好的 16 字节对齐槽 (dst, r2) 作为 kind 向量结果压栈. */
static void simd_finish_result(int kind, int dst, int r2)
{
    CType t = simd_vtype(kind);
    vset(&t, VT_LOCAL | VT_LVAL, dst);
    vtop->r2 = r2;              /* 标记槽在用, 防止被打包前复用 */
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
    } else {                      /* int: movdqa (66 prefix) */
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
    } else {                      /* int: movdqa (66 prefix) */
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
   0 add / 1 sub / 2 mul / 3 div. us=1 表示无符号除法(仅整型除法用 div vs idiv).
   F4/D2 走打包 addps~divps/addpd~divpd; 整型加/减/乘(若可用)走打包,
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

/* 位运算 opcode: and/or/xor/andnot. 对 ps/pd 同值. */
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

/* 原生运算符路由: vtop[-1]/vtop[0] 同为某一 SIMD 向量类型时, 对 + - * / 发射
   打包指令. 返回 1=已处理 (此时已压入结果), 0=不是 SIMD 运算. */
ST_FUNC int simd_gen_op(int op)
{
    int k, idx;
    k = simd_vector_kind(&vtop[-1].type);
    if (k < 0 || k != simd_vector_kind(&vtop[0].type))
        return 0;
    if (op == '+')       idx = 0;
    else if (op == '-')  idx = 1;
    else if (op == '*')  idx = 2;
    else if (op == '/')  idx = 3;
    else
        return 0;
    simd_emit_binop(k, idx, 0);
    return 1;
}

/* _mm_* 内建函数调度. 返回 1=已处理, 0=tok 不是 SIMD 内建. */
ST_FUNC int simd_builtin_dispatch(int tok)
{
    switch (tok) {
    case TOK_mm_load_ps:   parse_builtin_params(0, "e"); simd_emit_load(SIMD_F4); return 1;
    case TOK_mm_load_pd:   parse_builtin_params(0, "e"); simd_emit_load(SIMD_D2); return 1;
    case TOK_mm_load_epi32:parse_builtin_params(0, "e"); simd_emit_load(SIMD_I4); return 1;
    case TOK_mm_load_epi16:parse_builtin_params(0, "e"); simd_emit_load(SIMD_W16); return 1;
    case TOK_mm_load_epi8: parse_builtin_params(0, "e"); simd_emit_load(SIMD_B8); return 1;
    case TOK_mm_store_ps:  parse_builtin_params(0, "ee"); simd_emit_store(SIMD_F4); return 1;
    case TOK_mm_store_pd:  parse_builtin_params(0, "ee"); simd_emit_store(SIMD_D2); return 1;
    case TOK_mm_store_epi32:parse_builtin_params(0,"ee"); simd_emit_store(SIMD_I4); return 1;
    case TOK_mm_store_epi16:parse_builtin_params(0,"ee"); simd_emit_store(SIMD_W16); return 1;
    case TOK_mm_store_epi8: parse_builtin_params(0,"ee"); simd_emit_store(SIMD_B8); return 1;
    case TOK_mm_add_ps:  case TOK_mm_sub_ps:  case TOK_mm_mul_ps:  case TOK_mm_div_ps:
        { int simd_op = tok - TOK_mm_add_ps;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_F4, simd_op, 0); }
        return 1;
    case TOK_mm_add_pd:  case TOK_mm_sub_pd:  case TOK_mm_mul_pd:  case TOK_mm_div_pd:
        { int simd_op = tok - TOK_mm_add_pd;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_D2, simd_op, 0); }
        return 1;
    case TOK_mm_add_epi32: case TOK_mm_sub_epi32: case TOK_mm_mul_epi32: case TOK_mm_div_epi32:
        { int simd_op = tok - TOK_mm_add_epi32;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_I4, simd_op, 0); }
        return 1;
    case TOK_mm_add_epi16: case TOK_mm_sub_epi16: case TOK_mm_mul_epi16: case TOK_mm_div_epi16:
        { int simd_op = tok - TOK_mm_add_epi16;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_W16, simd_op, 0); }
        return 1;
    case TOK_mm_add_epi8:  case TOK_mm_sub_epi8:  case TOK_mm_mul_epi8:  case TOK_mm_div_epi8:
        { int simd_op = tok - TOK_mm_add_epi8;
          parse_builtin_params(0, "ee");
          simd_emit_binop(SIMD_B8, simd_op, 0); }
        return 1;
    case TOK_mm_div_epu32: parse_builtin_params(0, "ee"); simd_emit_binop(SIMD_I4, 3, 1); return 1;
    case TOK_mm_div_epu16: parse_builtin_params(0, "ee"); simd_emit_binop(SIMD_W16, 3, 1); return 1;
    case TOK_mm_div_epu8:  parse_builtin_params(0, "ee"); simd_emit_binop(SIMD_B8, 3, 1); return 1;
    case TOK_mm_setzero_ps:   parse_builtin_params(0, ""); simd_emit_vzero(SIMD_F4); return 1;
    case TOK_mm_setzero_pd:   parse_builtin_params(0, ""); simd_emit_vzero(SIMD_D2); return 1;
    case TOK_mm_setzero_epi32:parse_builtin_params(0, ""); simd_emit_vzero(SIMD_I4); return 1;
    case TOK_mm_setzero_epi16:parse_builtin_params(0, ""); simd_emit_vzero(SIMD_W16); return 1;
    case TOK_mm_setzero_epi8: parse_builtin_params(0, ""); simd_emit_vzero(SIMD_B8); return 1;
    /* ---- SIMD 常用扩展: 位运算 ---- */
    case TOK_mm_and_ps: case TOK_mm_or_ps: case TOK_mm_xor_ps: case TOK_mm_andnot_ps:
        { int simd_op = tok - TOK_mm_and_ps;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_F4, simd_bitop[simd_op], 0); }
        return 1;
    case TOK_mm_and_pd: case TOK_mm_or_pd: case TOK_mm_xor_pd: case TOK_mm_andnot_pd:
        { int simd_op = tok - TOK_mm_and_pd;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_D2, simd_bitop[simd_op], 1); }
        return 1;
    case TOK_mm_and_si128: case TOK_mm_or_si128: case TOK_mm_xor_si128: case TOK_mm_andnot_si128:
        { int simd_op = tok - TOK_mm_and_si128;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_I4, simd_bitop_i[simd_op], 1); }
        return 1;
    /* ---- min/max / sqrt ---- */
    case TOK_mm_min_ps: case TOK_mm_max_ps:
        { int simd_op = tok - TOK_mm_min_ps;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_F4, simd_minmax_f[simd_op], 0); }
        return 1;
    case TOK_mm_min_pd: case TOK_mm_max_pd:
        { int simd_op = tok - TOK_mm_min_pd;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_D2, simd_minmax_f[simd_op], 1); }
        return 1;
    case TOK_mm_sqrt_ps: parse_builtin_params(0, "e"); simd_emit_sqrt(SIMD_F4); return 1;
    case TOK_mm_sqrt_pd: parse_builtin_params(0, "e"); simd_emit_sqrt(SIMD_D2); return 1;
    /* ---- 比较 (float) ---- */
    case TOK_mm_cmpeq_ps: case TOK_mm_cmpneq_ps: case TOK_mm_cmplt_ps: case TOK_mm_cmple_ps:
    case TOK_mm_cmpgt_ps: case TOK_mm_cmpge_ps:
        { int simd_cmp = tok - TOK_mm_cmpeq_ps;
          parse_builtin_params(0, "ee");
          simd_emit_cmpf(SIMD_F4, simd_cmpf_imm[simd_cmp], simd_cmpf_swap[simd_cmp]); }
        return 1;
    case TOK_mm_cmpeq_pd: case TOK_mm_cmpneq_pd: case TOK_mm_cmplt_pd: case TOK_mm_cmple_pd:
    case TOK_mm_cmpgt_pd: case TOK_mm_cmpge_pd:
        { int simd_cmp = tok - TOK_mm_cmpeq_pd;
          parse_builtin_params(0, "ee");
          simd_emit_cmpf(SIMD_D2, simd_cmpf_imm[simd_cmp], simd_cmpf_swap[simd_cmp]); }
        return 1;
    /* ---- 比较 / min/max (int32) ---- */
    case TOK_mm_cmpeq_epi32: parse_builtin_params(0, "ee"); simd_emit_cmpi(0x76, 0); return 1;
    case TOK_mm_cmpgt_epi32: parse_builtin_params(0, "ee"); simd_emit_cmpi(0x66, 0); return 1;
    case TOK_mm_cmplt_epi32: parse_builtin_params(0, "ee"); simd_emit_cmpi(0x66, 1); return 1;
    case TOK_mm_min_epi32: case TOK_mm_max_epi32: case TOK_mm_min_epu32: case TOK_mm_max_epu32:
        { int simd_op = tok - TOK_mm_min_epi32;
          parse_builtin_params(0, "ee");
          simd_emit_membin(SIMD_I4, simd_minmax_i[simd_op], 2); }
        return 1;
    /* ---- 移位 (int32, imm) ---- */
    case TOK_mm_slli_epi32: parse_builtin_params(0, "ei"); simd_emit_shift(0xf0); return 1;
    case TOK_mm_srli_epi32: parse_builtin_params(0, "ei"); simd_emit_shift(0xd0); return 1;
    case TOK_mm_srai_epi32: parse_builtin_params(0, "ei"); simd_emit_shift(0xe0); return 1;
    /* ---- 类型转换 int32<->float32 ---- */
    case TOK_mm_cvtepi32_ps: parse_builtin_params(0, "e"); simd_emit_i4f4(); return 1;
    case TOK_mm_cvtps_epi32: parse_builtin_params(0, "e"); simd_emit_f4i4(0); return 1;
    case TOK_mm_cvttps_epi32: parse_builtin_params(0, "e"); simd_emit_f4i4(1); return 1;
    default:
        return 0;
    }
}

#endif /* TCC_TARGET_X86_64 */