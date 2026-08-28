/* matrix linalg.h - 直接法: LU(PartialPivLU) → 逆/行列式 + Cholesky(LLT) (M6)
 *
 * 作用对象: 方阵 `mt_mat(T,N,N)`, T ∈ {float,double}。全标量计算(不依赖 packet),
 * 保数值算法可读; 后续可在已调度 TCC __m128 后再向量化。
 *
 * 关键语义(对标 Eigen PartialPivLU / LLT):
 *   - mt_lu_factor:  就地 Doolittle LU + **部分选主元**, A 覆盖为 [L(单位下三角, 含隐
 *                     对角1) | U(上三角)] 合并存储; piv 记录行置换(P A = L U), sign=det(P)。
 *   - mt_mat_det:    沿 LU 阶 U 对角连乘 × sign → det(A)。奇异(U 对角遇 0)返回 0。
 *   - mt_mat_inverse: 对 N 个单位列应用"L y = P e_j, U x = y"回代, 组装 A^{-1}。
 *   - mt_cholesky:   **就地** A ← L(严格下三角+对角), 满足 A=LL^T; 非正定返回非0。
 *
 * 数值约定: 选主元以 |diag|==0 判奇异(fabs); Cholesky 以对角修正项 <=0 判非正定。
 * 就地/别名: inverse/cholesky 支持就地; lu_factor/inverse 内部用栈内同型临时,
 * 不破坏输入(除就地形式)。
 */
#ifndef MT_MAT_LINALG_H
#define MT_MAT_LINALG_H

#include "matrix.h"
#include <math.h>

/* 选主元绝对值 / Cholesky 开平方(T 泛型分派) */
#define MT_LIN_FABS(T) (_Generic((T)0, float: fabsf, double: fabs, default: 0))
#define MT_LIN_SQRT(T) (_Generic((T)0, float: sqrtf, double: sqrt, default: 0))

/* LU 分解(就地)。返回 0=成功, 1=奇异。
 * A 进入: 任意方阵; A 离开: 合并 [L(单位下三角) | U(上三角)]。P A = L U, P 由 piv 描述。 */
model (T, int N) int mt_lu_factor(mt_mat(T,N,N) *A, int piv[N], int *sign) {
    T (*Fa)(T) = MT_LIN_FABS(T);
    int i, j, k;
    if (sign) *sign = 1;
    for (i = 0; i < N; i++) piv[i] = i;
    for (k = 0; k < N; k++) {
        /* 选主元: 列 k 的 [k..N) 找 |A[i][k]| 最大行 */
        int pm = k; T bm = Fa(A->a[k * N + k]);
        for (i = k + 1; i < N; i++) {
            T v = Fa(A->a[i * N + k]);
            if (v > bm) { bm = v; pm = i; }
        }
        if (Fa(A->a[pm * N + k]) == 0) return 1;        /* 奇异: 该列主元为 0 */
        if (pm != k) {                                  /* 交换行 k/pm */
            for (j = 0; j < N; j++) {
                T t = A->a[k * N + j]; A->a[k * N + j] = A->a[pm * N + j]; A->a[pm * N + j] = t;
            }
            { int t = piv[k]; piv[k] = piv[pm]; piv[pm] = t; }
            if (sign) *sign = -*sign;
        }
        T piv_v = A->a[k * N + k];
        for (i = k + 1; i < N; i++) {
            A->a[i * N + k] /= piv_v;                   /* 存 L 乘数(单位下三角隠对角1) */
        }
        for (i = k + 1; i < N; i++) {                   /* U: 消元右下角 */
            T lm = A->a[i * N + k];
            for (j = k + 1; j < N; j++)
                A->a[i * N + j] -= lm * A->a[k * N + j];
        }
    }
    return 0;
}

/* --- LU 回代 ---
 * 对单 RHS r(已作 P e_j 置换), 解 L y = r(forward), U x = y(backward), 结果存 x。
 * 在 mt_mat_inverse 内联使用。 */

/* 行列式: det(A) = sign × ∏ U_ii (U 对角取自 in-place LU 的 diag) */
model (T, int N) T mt_mat_det(const mt_mat(T,N,N) *A) {
    mt_mat(T,N,N) w;
    int piv[N], i, sign;
    for (i = 0; i < N * N; i++) w.a[i] = A->a[i];
    if (mt_lu_factor(T,N)(&w, piv, &sign)) return 0;        /* 奇异 → 0 */
    T det = (T)sign;
    for (i = 0; i < N; i++) det *= w.a[i * N + i];
    return det;
}

/* 逆矩阵: dst = A^{-1}(N 个单位解). 返回 0=成功, 1=奇异。dst 可与 A 同址。 */
model (T, int N) int mt_mat_inverse(mt_mat(T,N,N) *dst, const mt_mat(T,N,N) *A) {
    mt_mat(T,N,N) w;
    int piv[N], sign, i, j, s;
    int n = N;
    for (i = 0; i < n * n; i++) w.a[i] = A->a[i];
    if (mt_lu_factor(T,N)(&w, piv, &sign)) return 1;
    /* 每列 j: 求解 A x = e_j */
    for (j = 0; j < n; j++) {
        T r[N], y[N];                                       /* RHS(置换后) 与工作向量 */
        for (i = 0; i < n; i++) r[i] = (piv[i] == j) ? (T)1 : (T)0;  /* P e_j: r[i]=e_j[piv[i]] */
        /* forward: L y = r (L 单位下三角, 对角1) */
        for (i = 0; i < n; i++) {
            T acc = r[i];
            for (s = 0; s < i; s++) acc -= w.a[i * n + s] * y[s];
            y[i] = acc;
        }
        /* backward: U x = y (U 上三角含对角) */
        for (i = n - 1; i >= 0; i--) {
            T acc = y[i];
            for (s = i + 1; s < n; s++) acc -= w.a[i * n + s] * dst->a[s * n + j];
            dst->a[i * n + j] = acc / w.a[i * n + i];
        }
    }
    return 0;
}

/* Cholesky(LLT): **就地** A ← L(A = LL^T, L 下三角含对角)。对角修正 <=0 判非正定 → 1。 */
model (T, int N) int mt_cholesky(mt_mat(T,N,N) *A) {
    T (*Sq)(T) = MT_LIN_SQRT(T);
    int i, j, s;
    for (j = 0; j < N; j++) {
        T dp = A->a[j * N + j];
        for (s = 0; s < j; s++) dp -= A->a[j * N + s] * A->a[j * N + s];
        if (dp <= 0) return 1;                              /* 非正定 */
        T ljj = Sq(dp);
        A->a[j * N + j] = ljj;
        for (i = j + 1; i < N; i++) {                       /* 第 j 列下三角 */
            T acc = A->a[i * N + j];
            for (s = 0; s < j; s++) acc -= A->a[i * N + s] * A->a[j * N + s];
            A->a[i * N + j] = acc / ljj;
        }
    }
    return 0;
}

#endif /* MT_MAT_LINALG_H */