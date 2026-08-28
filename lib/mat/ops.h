/* matrix ops.h - 算子与融合求值通道 (M1)
 *
 * 核心语义(见 docs/matrix.md §6/§7): `dst = expr` 在**单个循环**内逐元素写入 dst,
 * 不层层物化临时 —— 对齐 Eigen CwiseBinaryOp + AssignEvaluator 的"算子→一次求值"。
 *
 * 值模型: 纯算子对象 `mt_expr(T)`, 全字段只读; 叶子 SRC/BCAST, 复合 ADD/SUB/MUL/
 * SCAL/UNARY 组合 ln/rn/scalar/un。求值 = 根 desc 递归 get(逐元素)。
 *
 * 生命周期(不悬挂, §3.4): 表达式节点一律由**调用方栈上构造并在同一作用域内消费**
 * (同一语句/同一块内建节点→eval)。叶子引用左值矩阵的 `a`, 复合引用调用方局部节点
 * —— 不求值结束后存活, 故无悬挂。这是 P0 通道形态; P1(§8)再引入指针树懒求值。
 *
 * 约定: 实例化点显式 `mt_mat_*(T,R,C)(...)` / `mt_expr_*(T)(...)`。
 * 本层先标量逐元素(M2 替换为 packet 并行块)。
 */
#ifndef MT_MAT_OPS_H
#define MT_MAT_OPS_H

#include "matrix.h"
#include "packet.h"

/* 算子种类 (设计全集; 叶子 SRC/BCAST, 复合 ADD..UNARY, 视图 VIEW, REDUC 占位)。
 * VIEW 是对 §7 TRANSP 的泛化: 同一节点按 `trans` 区分「子块」与「转置」, 把输出
 * 线性下标 i 映射到源坐标, 求值期经 get 坐标映射 (§8, M4)。 */
enum MT_MatOpKind {
    MT_OP_SRC,       /* 叶子: 引用左值矩阵 a(线性第 i 元素) */
    MT_OP_BCAST,     /* 叶子: 标量广播 */
    MT_OP_ADD,       /* ln + rn */
    MT_OP_SUB,       /* ln - rn */
    MT_OP_MUL,       /* ln * rn */
    MT_OP_SCAL,      /* ln * scalar */
    MT_OP_UNARY,     /* un(ln) */
    MT_OP_VIEW,      /* 视图叶子: 源子块/转置, 坐标映射(§8/M4) */
    MT_OP_REDUC      /* (占位) 归约, M3 */
};

/* 纯算子: 求值期只读。指针子树(§2.C): 复合节点引用调用方局部子节点, 不内嵌拷贝。
 * VIEW 映射: 输出满足 dst(rows,_)-源, get(i) 由输出列数 cols 反推 (or,oc)=f(i),
 * 再经 srccols/ox/oy/trans 映射到源线性下标(见 mt_expr_get 中 MT_OP_VIEW 分支)。 */
model struct mt_expr(T) {
    int kind;
    const T *src;      /* SRC: 左值矩阵 a 地址(引用); VIEW: 源 a 地址 */
    T scalar;          /* BCAST/SCAL: 标量 */
    T (*un)(T);        /* UNARY: 一元函数(如 sqrtf/sqrt) */
    const mt_expr(T) *ln, *rn;   /* 复合: 左/右子节点(指针, 引用调用方局部) */
    /* VIEW 映射字段 (仅 kind==MT_OP_VIEW 有效) */
    int cols;          /* 输出列数: i -> or=i/cols, oc=i%cols */
    int srccols;       /* 源列数 */
    int ox, oy;        /* 源偏移 (列, 行) */
    int trans;         /* 1=转置映射; 0=子块正常映射 */
};

/* --- 叶子构造 --- */

/* src: 引用给定基址为源(供 SRC 叶子) */
model (T) mt_expr(T) mt_expr_src(const T *base) {
    mt_expr(T) e; e.kind = MT_OP_SRC; e.src = base; e.scalar = 0; e.un = 0;
    e.ln = 0; e.rn = 0; return e;
}
/* bcast: 标量广播 */
model (T) mt_expr(T) mt_expr_bcast(T s) {
    mt_expr(T) e; e.kind = MT_OP_BCAST; e.src = 0; e.scalar = s; e.un = 0;
    e.ln = 0; e.rn = 0; return e;
}

/* --- 复合构造(kind/ln/rn), 子节点按指针引用调用方局部节点 --- */

model (T) mt_expr(T) mt_expr_bin(int kind, const mt_expr(T) *l, const mt_expr(T) *r) {
    mt_expr(T) e; e.kind = kind; e.src = 0; e.scalar = 0; e.un = 0;
    e.ln = l; e.rn = r; return e;
}
/* scal: 标量乘(ln * s) */
model (T) mt_expr(T) mt_expr_scal(const mt_expr(T) *l, T s) {
    mt_expr(T) e; e.kind = MT_OP_SCAL; e.src = 0; e.scalar = s; e.un = 0;
    e.ln = l; e.rn = 0; return e;
}
/* un: 一元(ln -> un(ln)) */
model (T) mt_expr(T) mt_expr_un(const mt_expr(T) *e0, T (*fn)(T)) {
    mt_expr(T) e; e.kind = MT_OP_UNARY; e.src = 0; e.scalar = 0; e.un = fn;
    e.ln = e0; e.rn = 0; e.cols = 0; e.srccols = 0; e.ox = 0; e.oy = 0; e.trans = 0;
    return e;
}

/* --- 视图叶子构造 (M4, §7 TRANSP 泛化) ---
 * view(base, cols, srccols, ox, oy, trans): 输出形状由调用方按 dst 决定, 此处只
 * 给坐标映射所需参数:
 *   - cols     输出列数(决定 i -> or/oc)
 *   - srccols  源列数
 *   - ox, oy   源列/行偏移(子块裁剪)
 *   - trans    0=子块 dst[i]=src[oy* srccols + ox ..], 1=转置 dst[or][oc]=src[oc][or]
 * 转置本源: 输出 i, or=i/cols, oc=i%cols; 源下标 = trans? (oc+ox)*srccols+(or+oy)
 *                                           : (or+oy)*srccols+(oc+ox) */
model (T) mt_expr(T) mt_expr_view(const T *base, int cols, int srccols,
                                  int ox, int oy, int trans) {
    mt_expr(T) e;
    e.kind = MT_OP_VIEW; e.src = base; e.scalar = 0; e.un = 0;
    e.ln = 0; e.rn = 0;
    e.cols = cols; e.srccols = srccols; e.ox = ox; e.oy = oy; e.trans = trans;
    return e;
}
/* transpose: 源为 srcrows×srccols, 转置后输出形状 = srccols×srcrows, 故输出列数 cols=srccols */
model (T) mt_expr(T) mt_expr_transpose(const T *base, int srcrows, int srccols) {
    /* 输出 cols = srcrows(转置后多少列=源行数); srccols 保持源列数; trans=1 */
    return mt_expr_view(T)(base, srcrows, srccols, 0, 0, 1);
}
/* block: 子块视图, 输出 oy×ox 起步, 输出列数 outcols, 源列数 srccols */
model (T) mt_expr(T) mt_expr_block(const T *base, int outcols, int srccols,
                                   int ox, int oy) {
    return mt_expr_view(T)(base, outcols, srccols, ox, oy, 0);
}

/* --- 求值: 迭代后序取第 i 元素 (显式栈, 无 model 自递归) ---
 * 树深上限 MDEPS=64 (§14 已知限制)。语义等价于递归 get:
 *   get(x,i) = SRC? src[i] : BCAST? scalar : ADD? get(ln)+get(rn)
 *             : SUB? get(ln)-get(rn) : MUL? get(ln)*get(rn)
 *             : SCAL? get(ln)*scalar : UNARY? un(get(ln))
 * 手工后序: 栈帧 st[p] 配 stg[p]; 子完成把 r 向上递交, 父收满子即合成并弹出继续上递。
 * 用显式栈避免 model 函数自递归实例化(合成体内自调用替换成 synth 名后不再命中
 * 已登记 model → 编译失败, 见 §14)。*/
#define MT_EXPR_MDEPS 64
model (T) T mt_expr_get(const mt_expr(T) *root, int i) {
    const mt_expr(T) *st[MT_EXPR_MDEPS];
    int stg[MT_EXPR_MDEPS];   /* 0=待展开子; 1=已收第1子; 2=二元已收两子 */
    T pv[MT_EXPR_MDEPS];      /* pv[p] = 帧 p 已收孩子/累计 */
    int sp = 0;
    T r = 0;

    if (!root) { MT_ASSERT(0 && "mt_expr_get: 空表达式"); return 0; }
    st[0] = root; stg[0] = 0; pv[0] = 0; sp = 1;
    while (sp > 0) {
        const mt_expr(T) *e = st[sp - 1];
        int k = e->kind;
        if (k == MT_OP_SRC)              { r = e->src[i];          sp--; }
        else if (k == MT_OP_BCAST)       { r = e->scalar;          sp--; }
        else if (k == MT_OP_VIEW) {
            int or_ = i / e->cols, oc = i % e->cols;   /* 输出行/列 */
            int si = e->trans ? (oc + e->ox) * e->srccols + (or_ + e->oy)
                              : (or_ + e->oy) * e->srccols + (oc + e->ox);
            r = e->src[si]; sp--;
        }
        else if (k == MT_OP_UNARY || k == MT_OP_SCAL) {
            if (stg[sp - 1] == 0) {
                MT_ASSERT(sp < MT_EXPR_MDEPS && "mt_expr_get: 深度超 MT_EXPR_MDEPS");
                stg[sp - 1] = 1;
                st[sp] = e->ln; stg[sp] = 0; pv[sp] = 0; sp++;
                continue;                              /* 扩到子, 结果由递交回路送达 */
            }
            r = (k == MT_OP_UNARY) ? e->un(pv[sp]) : (pv[sp] * e->scalar);
            sp--;
        } else { /* 二元 ADD/SUB/MUL */
            if (stg[sp - 1] == 0) {
                MT_ASSERT(sp < MT_EXPR_MDEPS && "mt_expr_get: 深度超 MT_EXPR_MDEPS");
                stg[sp - 1] = 1;
                st[sp] = e->ln; stg[sp] = 0; pv[sp] = 0; sp++;
                continue;
            } else if (stg[sp - 1] == 1) {
                /* 首子已到(递交回路已把 r 存入 pv[sp-1]), 只置 stg=2, 勿覆盖 pv[sp-1] */
                MT_ASSERT(sp < MT_EXPR_MDEPS && "mt_expr_get: 深度超 MT_EXPR_MDEPS");
                stg[sp - 1] = 2;
                st[sp] = e->rn; stg[sp] = 0; pv[sp] = 0; sp++;
                continue;
            }
            r = pv[sp - 1]; sp--;          /* 通常经递交回路弹出, 此处兜底 */
        }
        /* 向上递交 r 直至栈空或遇阻塞父帧 */
        for (;;) {
            if (sp == 0) return r;
            e = st[sp - 1]; k = e->kind;
            if (k == MT_OP_UNARY || k == MT_OP_SCAL) {
                if (stg[sp - 1] == 1) {                 /* 唯一子已到: 合成并弹出上递 */
                    r = (k == MT_OP_UNARY) ? e->un(r) : (r * e->scalar);
                    sp--; continue;
                }
                pv[sp - 1] = r; break;                  /* 等子(异常路径) */
            }
            /* 二元 */
            if (stg[sp - 1] == 1) { pv[sp - 1] = r; break; }   /* 收第1子, 等第2子 */
            /* stg==2: 第2子到: 与 pv(第1子)合成并弹出上递 */
            { T a = pv[sp - 1], b = r;
              r = (k == MT_OP_ADD) ? a + b : (k == MT_OP_SUB) ? a - b : a * b;
              sp--; continue; }
        }
    }
    return r;
}

/* --- 融合单通道 eval: dst = expr, 单循环逐元素写入 ---
 * 返回 0 = 成功。M2 将把"对齐块内连续元素"替换为 packet 并行。 */
model (T, int R, int C) int mt_mat_eval(mt_mat(T,R,C) *dst, const mt_expr(T) *e) {
    int i;
    MT_ASSERT(dst && e);
    for (i = 0; i < R * C; i++) dst->a[i] = mt_expr_get(T)(e, i);
    return 0;
}

/* ============================================================
 * 便捷算子 (§6: 立即求值, dst = a op b)。每个算子在调用帧内构造节点并当场消费,
 * 符合"同一语句/块内构造并消费"的不悬挂约定 (§3.4)。
 * ============================================================ */

/* dst = a + b (逐元素; 能向量化时走 packet 单通道, 否则回退表达式求值) */
model (T, int R, int C) void mt_mat_add(mt_mat(T,R,C) *dst,
                                         const mt_mat(T,R,C) *a,
                                         const mt_mat(T,R,C) *b) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_add(T)(dst->a, a->a, b->a, R * C);
        return;
    }
    mt_expr(T) la = mt_expr_src(T)(a->a);
    mt_expr(T) ra = mt_expr_src(T)(b->a);
    mt_expr(T) root = mt_expr_bin(T)(MT_OP_ADD, &la, &ra);
    (void) mt_mat_eval(T,R,C)(dst, &root);
}
/* dst = a - b */
model (T, int R, int C) void mt_mat_sub(mt_mat(T,R,C) *dst,
                                         const mt_mat(T,R,C) *a,
                                         const mt_mat(T,R,C) *b) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_sub(T)(dst->a, a->a, b->a, R * C);
        return;
    }
    mt_expr(T) la = mt_expr_src(T)(a->a);
    mt_expr(T) ra = mt_expr_src(T)(b->a);
    mt_expr(T) root = mt_expr_bin(T)(MT_OP_SUB, &la, &ra);
    (void) mt_mat_eval(T,R,C)(dst, &root);
}
/* dst = a * b (逐元素) */
model (T, int R, int C) void mt_mat_mul(mt_mat(T,R,C) *dst,
                                         const mt_mat(T,R,C) *a,
                                         const mt_mat(T,R,C) *b) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_mul(T)(dst->a, a->a, b->a, R * C);
        return;
    }
    mt_expr(T) la = mt_expr_src(T)(a->a);
    mt_expr(T) ra = mt_expr_src(T)(b->a);
    mt_expr(T) root = mt_expr_bin(T)(MT_OP_MUL, &la, &ra);
    (void) mt_mat_eval(T,R,C)(dst, &root);
}
/* dst = a * s (标量广播) */
model (T, int R, int C) void mt_mat_scal(mt_mat(T,R,C) *dst,
                                          const mt_mat(T,R,C) *a, T s) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_scal(T)(dst->a, a->a, s, R * C);
        return;
    }
    mt_expr(T) la = mt_expr_src(T)(a->a);
    mt_expr(T) root = mt_expr_scal(T)(&la, s);
    (void) mt_mat_eval(T,R,C)(dst, &root);
}
/* dst = un(a) (逐元素一元; fn 如 sqrtf/sqrt) */
model (T, int R, int C) void mt_mat_un(mt_mat(T,R,C) *dst,
                                        const mt_mat(T,R,C) *a, T (*fn)(T)) {
    mt_expr(T) la = mt_expr_src(T)(a->a);
    mt_expr(T) root = mt_expr_un(T)(&la, fn);
    (void) mt_mat_eval(T,R,C)(dst, &root);
}

/* ============================================================
 * 逐元素数组语义 (M3; 数组语义 = unary/binary un op 变体, 可向量化走 packet)
 * ============================================================ */

/* dst = |a| (逐元素绝对值) */
model (T, int R, int C) void mt_mat_abs(mt_mat(T,R,C) *dst,
                                         const mt_mat(T,R,C) *a) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_abs(T)(dst->a, a->a, R * C);
        return;
    }
    int i;
    for (i = 0; i < R * C; i++) dst->a[i] = a->a[i] < 0 ? -a->a[i] : a->a[i];
}
/* dst = sqrt(a) (逐元素) */
model (T, int R, int C) void mt_mat_sqrt(mt_mat(T,R,C) *dst,
                                          const mt_mat(T,R,C) *a) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_sqrt(T)(dst->a, a->a, R * C);
        return;
    }
    int i;
    for (i = 0; i < R * C; i++) dst->a[i] = (T)sqrt((double)a->a[i]);
}
/* dst = coeffwise_min(a,b) (逐元素取小) */
model (T, int R, int C) void mt_mat_cmin(mt_mat(T,R,C) *dst,
                                          const mt_mat(T,R,C) *a,
                                          const mt_mat(T,R,C) *b) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_min(T)(dst->a, a->a, b->a, R * C);
        return;
    }
    int i;
    for (i = 0; i < R * C; i++) dst->a[i] = a->a[i] < b->a[i] ? a->a[i] : b->a[i];
}
/* dst = coeffwise_max(a,b) (逐元素取大) */
model (T, int R, int C) void mt_mat_cmax(mt_mat(T,R,C) *dst,
                                          const mt_mat(T,R,C) *a,
                                          const mt_mat(T,R,C) *b) {
    if (mt_packet_can_vec(T)(dst->a, R * C)) {
        mt_mat_vec_max(T)(dst->a, a->a, b->a, R * C);
        return;
    }
    int i;
    for (i = 0; i < R * C; i++) dst->a[i] = a->a[i] > b->a[i] ? a->a[i] : b->a[i];
}

/* ============================================================
 * 归约 (M3; 单趟归约, 语义见 docs/matrix.md §1.2) —— 输出标量 T
 * ============================================================ */

/* sum: 全元素和 */
model (T, int R, int C) T mt_mat_sum(const mt_mat(T,R,C) *a) {
    return mt_mat_hsum(T)(a->a, R * C);
}
/* mean: 算术平均 = sum / n */
model (T, int R, int C) T mt_mat_mean(const mt_mat(T,R,C) *a) {
    return (T)(mt_mat_hsum(T)(a->a, R * C) / (T)(R * C));
}
/* sqnorm: 平方和 = <a,a> (L2 范数平方) */
model (T, int R, int C) T mt_mat_sqnorm(const mt_mat(T,R,C) *a) {
    return mt_mat_hdot(T)(a->a, a->a, R * C);
}
/* norm: L2 范数 = sqrt(sqnorm) */
model (T, int R, int C) T mt_mat_norm(const mt_mat(T,R,C) *a) {
    T s = mt_mat_sqnorm(T,R,C)(a);
    return _Generic((T)0,
        float:  sqrtf(s),
        double: sqrt(s),
        default: (T)0);
}
/* dot: 逐元素点积 <a,b> (同形状) */
model (T, int R, int C) T mt_mat_dot(const mt_mat(T,R,C) *a,
                                     const mt_mat(T,R,C) *b) {
    return mt_mat_hdot(T)(a->a, b->a, R * C);
}
/* trace: 迹 = sum 主对角线 (仅方形 R==C, 由调用方保证) */
model (T, int R, int C) T mt_mat_trace(const mt_mat(T,R,C) *a) {
    int i; T s = 0;
    MT_ASSERT(R == C);
    for (i = 0; i < R; i++) s += a->a[i * C + i];
    return s;
}
/* min: 最小元素 */
model (T, int R, int C) T mt_mat_min(const mt_mat(T,R,C) *a) {
    return mt_mat_hmin(T)(a->a, R * C);
}
/* max: 最大元素 */
model (T, int R, int C) T mt_mat_max(const mt_mat(T,R,C) *a) {
    return mt_mat_hmax(T)(a->a, R * C);
}

/* ============================================================
 * M4: P1 懒求值 —— 调用方一次性缓冲 `mt_expr_frame` (§8)
 *
 * 现状(P0, M1): 表达式节点由调用方用多个 **具名局部变量** 逐个构造再 eval, 对深复合
 * (如 A*B+X*Y) 需要调用方逐层手工命名, 啰嗦且易错。
 * P1(M4): 把"调用方局部节点"打包为一个 **frame(定尺寸节点槽数组, 栈上)**, 在求值前
 * 原地建槽、用槽 index 出根; eval 取根槽求值。特性:
 *   - 零堆: 栈上定长数组, 无 malloc/free;
 *   - 无悬挂: frame 同作用域内构造并消费, 求值后即失效;
 *   - 深度受限: 槽数 ≤ MT_EXPR_FRAME_MAX(超限断言, §14 扇出兜底保留);
 *   - 相对 P0 引入的落叶引用左值 `a`, 复合节点经槽 index 引用同 frame 内兄弟槽指针。
 *
 * frame 内槽即 mt_expr(T), 槽间用指针互链(frame 是稳定数组, 地址恒稳)。
 * ============================================================ */

#define MT_EXPR_FRAME_MAX 48

model struct mt_expr_frame(T) {
    mt_expr(T) n[MT_EXPR_FRAME_MAX];
    int used;          /* 已占槽数; 每建一槽递增, 超限断言 */
};

/* 帧复位: 清空缓冲区(新表达式线框) */
model (T) void mt_expr_frame_clear(mt_expr_frame(T) *f) {
    f->used = 0;
}
/* 建槽 SRC(引用左值 a) */
model (T) int mt_expr_slot_src(mt_expr_frame(T) *f, const T *base) {
    MT_ASSERT(f->used < MT_EXPR_FRAME_MAX);
    f->n[f->used] = mt_expr_src(T)(base); return f->used++;
}
/* 建槽 BCAST(标量广播) */
model (T) int mt_expr_slot_bcast(mt_expr_frame(T) *f, T s) {
    MT_ASSERT(f->used < MT_EXPR_FRAME_MAX);
    f->n[f->used] = mt_expr_bcast(T)(s); return f->used++;
}
/* 建槽 一元/标量乘/复合二元(l,r 为 frame 内已有槽 index) */
model (T) int mt_expr_slot_un(mt_expr_frame(T) *f, int l, T (*fn)(T)) {
    MT_ASSERT(f->used < MT_EXPR_FRAME_MAX);
    f->n[f->used] = mt_expr_un(T)(&f->n[l], fn); return f->used++;
}
model (T) int mt_expr_slot_scal(mt_expr_frame(T) *f, int l, T s) {
    MT_ASSERT(f->used < MT_EXPR_FRAME_MAX);
    f->n[f->used] = mt_expr_scal(T)(&f->n[l], s); return f->used++;
}
model (T) int mt_expr_slot_bin(mt_expr_frame(T) *f, int kind, int l, int r) {
    MT_ASSERT(f->used < MT_EXPR_FRAME_MAX);
    f->n[f->used] = mt_expr_bin(T)(kind, &f->n[l], &f->n[r]); return f->used++;
}
/* 建槽 VIEW(cols/srccols/ox/oy/trans 同 mt_expr_view) */
model (T) int mt_expr_slot_view(mt_expr_frame(T) *f, const T *base,
                                int cols, int srccols, int ox, int oy, int trans) {
    MT_ASSERT(f->used < MT_EXPR_FRAME_MAX);
    f->n[f->used] = mt_expr_view(T)(base, cols, srccols, ox, oy, trans);
    return f->used++;
}
/* 帧求值: dst = 槽 root 描述的表达式, 单循环逐元素写入(与 mt_mat_eval 同语义) */
model (T, int R, int C) int mt_mat_eval_frame(mt_mat(T,R,C) *dst,
                                              const mt_expr_frame(T) *f, int root) {
    int i;
    MT_ASSERT(dst && root >= 0 && root < f->used);
    for (i = 0; i < R * C; i++) dst->a[i] = mt_expr_get(T)(&f->n[root], i);
    return 0;
}

/* --- P1 便捷: 懒转置 / 懒块视图 (dst 形状决定输出; 内部走 VIEW 叶子) --- */

/* dst(C×R) = src(R×C)^T, 经懒求值单通道 */
model (T, int R, int C) void mt_mat_eval_transpose(mt_mat(T,C,R) *dst,
                                                    const mt_mat(T,R,C) *src) {
    mt_expr(T) root = mt_expr_transpose(T)(src->a, R, C);
    (void) mt_mat_eval(T,C,R)(dst, &root);
}
/* dst(R×C) = src 的子块: 源为 sc 列(纵长不限), 起点(列 ox, 行 oy), 输出列数 C。
 * 源形状不进 model 类型参(dst 的形状已由 R/C 决定), 故源经裸指针 + 运行时源列数 sc
 * 传入; 调用方传 src.a。调用方确保 ox+C<=sc 且 oy+R<=源行数。 */
model (T, int R, int C) void mt_mat_eval_block(mt_mat(T,R,C) *dst,   /* R×C = 输出块形状 */
                                               const T *src, int sc,
                                               int ox, int oy) {
    mt_expr(T) root = mt_expr_block(T)(src, C, sc, ox, oy);
    (void) mt_mat_eval(T,R,C)(dst, &root);
}

/* ============================================================
 * M5: GEMM 矩阵乘 C(R×N) = A(R×K) · B(K×N) (§10)
 *
 * 形参: model (T,int R,int K,int N) —— dst 形状 R×N 决定输出, a 为 R×K, b 为 K×N。
 * 别名(§11): 若 dst 与 a 或 b 的内存区间重叠(如方阵就地 A = A·A), gemm 微核的
 * C 行就地累加会破坏源 → 先把重叠源复制到栈上对齐临时(尺寸编译期已知)再算。
 * ============================================================ */

/* dst(R×N) = a(R×K) · b(K×N)
 * __attribute__((noinline)): 关键 —— clang -O2/-O3 内联本函数后, 会基于 const
 * a/b 入参的别名重新排序, 误把就地乘(dst==a==b, §11 别名保护的「重叠备份→重算」)
 * 破坏成 A=A·A 出错 (t089 #6 met desugar)。禁止内联即保证重叠保护先于微核执行。
 * TCC model 泛型展开时, 该属性随实例保留(见 desugar 产物确认)。 */
model (T, int R, int K, int N) void
__attribute__((noinline))
mt_mat_prod(mt_mat(T,R,N) *dst,
            const mt_mat(T,R,K) *a,
            const mt_mat(T,K,N) *b) {
    /* 区间重叠检测(段级, 指针线性序比较; uintptr 处防 TCC 无关指针比较告警) */
    uintptr_t db = (uintptr_t)dst->a, de = db + (uintptr_t)(R * N) * sizeof(T);
    uintptr_t ab = (uintptr_t)a->a, ae = ab + (uintptr_t)(R * K) * sizeof(T);
    uintptr_t bb = (uintptr_t)b->a, be = bb + (uintptr_t)(K * N) * sizeof(T);
    int overlap_a = (db < ae) && (ab < de);
    int overlap_b = (db < be) && (bb < de);
    const T *A = a->a, *B = b->a;
    /* 任一源与 dst 重叠(含 a==b==dst 就地方阵): 受影响源先全部备份到栈临时再算。
     * 全部备份在写 dst 之前完成,读到的都是原值,故 ca/cb 可安全独立送入微核。
     * 注意: 仅备份**重叠的那个**源 —— 未重叠源仍直接用原指针, 不得更换为占位缓冲
     * (否则把未重叠源误换成全 0, 见 08-28 修复)。 */
    if (overlap_a || overlap_b) {
        T ca[R * K], cb[K * N]; int i;
        if (overlap_a) for (i = 0; i < R * K; i++) ca[i] = a->a[i];
        if (overlap_b) for (i = 0; i < K * N; i++) cb[i] = b->a[i];
        A = overlap_a ? ca : a->a;
        B = overlap_b ? cb : b->a;
    }
    mt_mat_gemm(T)(dst->a, A, B, R, K, N);
}

#endif /* MT_MAT_OPS_H */