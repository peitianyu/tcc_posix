/* matrix packet.h - SIMD packet 抽象层 (M2)
 *
 * 对标 Eigen packet_traits → find_best_packet: 上层**算法只依赖**下面这套语义, SSE
 * 落地在 pack_sse.h(未来 HIP/CUDA 各自变体), 调用点不变、只换后端。
 *
 *    mt_packet_lanes(T)()        : 每 packet 元素数(float 4 / double 2; 其它类型 0=无 SIMD)
 *    mt_packet_can_vec(T)(p,n)   : 能否向量化处理 n 个连续元素(类型有后端 & n 为 lane
 *                                  整数倍 & 基址 16B 对齐)。对齐前提由 mt_mat 类型级
 *                                  16B 保证(见 matrix.h)。
 *    mt_mat_vec_add/sub/mul/sqrt(T)(r,a[,b],n)  : 向量算子, load+op+store 并把尾部标量化
 *    mt_mat_vec_scal(T)(r,a,s,n) : 标量广播乘(种子来自 alignas 16 数组 + 对齐 load)
 *
 * 本层**自包含**: 不引入 lib/stl。类型无 SIMD 后端(int 等)时分派走标量兜底, 语义与
 * 逐元素完全等价 —— 正确性不依赖 SIMD 是否启用。
 */
#ifndef MT_MAT_PACKET_H
#define MT_MAT_PACKET_H

#include "matrix.h"
#include "pack_sse.h"
#include <stdint.h>

/* 每 packet 元素数; 0 = 该类型无 SIMD 后端(走标量兜底)。 */
model (T) int mt_packet_lanes(void) {
    return _Generic((T)0, float: 4, double: 2, default: 0);
}

/* 能否向量化: n>=lane 且 n%lane==0 且基址 16B 对齐。 */
model (T) int mt_packet_can_vec(const void *p, int n) {
    int L = mt_packet_lanes(T)();
    if (L <= 0 || n < L) return 0;
    if (n % L != 0) return 0;
    return (((uintptr_t)p) & 15u) == 0;
}

/* --- 向量算子: 自含分派(float→SSE F4, double→SSE D2, 其它→标量兜底) --- */

/* r = a + b (n 个连续元素) */
model (T) void mt_mat_vec_add(void *r, const void *a, const void *b, int n) {
    void (*f)(void *, const void *, const void *, int) =
        _Generic((T)0, float: mt_sse_add_f, double: mt_sse_add_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; const T *bb = (const T *)b; int i;
    if (f) { f(rr, aa, bb, n); return; }
    for (i = 0; i < n; i++) rr[i] = aa[i] + bb[i];
}
/* r = a - b */
model (T) void mt_mat_vec_sub(void *r, const void *a, const void *b, int n) {
    void (*f)(void *, const void *, const void *, int) =
        _Generic((T)0, float: mt_sse_sub_f, double: mt_sse_sub_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; const T *bb = (const T *)b; int i;
    if (f) { f(rr, aa, bb, n); return; }
    for (i = 0; i < n; i++) rr[i] = aa[i] - bb[i];
}
/* r = a * b (逐元素) */
model (T) void mt_mat_vec_mul(void *r, const void *a, const void *b, int n) {
    void (*f)(void *, const void *, const void *, int) =
        _Generic((T)0, float: mt_sse_mul_f, double: mt_sse_mul_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; const T *bb = (const T *)b; int i;
    if (f) { f(rr, aa, bb, n); return; }
    for (i = 0; i < n; i++) rr[i] = aa[i] * bb[i];
}
/* r = a * s (标量广播) */
model (T) void mt_mat_vec_scal(void *r, const void *a, T s, int n) {
    void (*f)(void *, const void *, T, int) =
        _Generic((T)0, float: mt_sse_scal_f, double: mt_sse_scal_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; int i;
    if (f) { f(rr, aa, s, n); return; }
    for (i = 0; i < n; i++) rr[i] = aa[i] * s;
}
/* r = sqrt(a) (逐元素) */
model (T) void mt_mat_vec_sqrt(void *r, const void *a, int n) {
    void (*f)(void *, const void *, int) =
        _Generic((T)0, float: mt_sse_sqrt_f, double: mt_sse_sqrt_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; int i;
    if (f) { f(rr, aa, n); return; }
    for (i = 0; i < n; i++) rr[i] = (T)sqrt(aa[i]);
}
/* r = |a| (逐元素) */
model (T) void mt_mat_vec_abs(void *r, const void *a, int n) {
    void (*f)(void *, const void *, int) =
        _Generic((T)0, float: mt_sse_abs_f, double: mt_sse_abs_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; int i;
    if (f) { f(rr, aa, n); return; }
    for (i = 0; i < n; i++) rr[i] = aa[i] < 0 ? -aa[i] : aa[i];
}
/* r = min(a,b) (逐元素) */
model (T) void mt_mat_vec_min(void *r, const void *a, const void *b, int n) {
    void (*f)(void *, const void *, const void *, int) =
        _Generic((T)0, float: mt_sse_min_f, double: mt_sse_min_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; const T *bb = (const T *)b; int i;
    if (f) { f(rr, aa, bb, n); return; }
    for (i = 0; i < n; i++) rr[i] = aa[i] < bb[i] ? aa[i] : bb[i];
}
/* r = max(a,b) (逐元素) */
model (T) void mt_mat_vec_max(void *r, const void *a, const void *b, int n) {
    void (*f)(void *, const void *, const void *, int) =
        _Generic((T)0, float: mt_sse_max_f, double: mt_sse_max_d, default: 0);
    T *rr = (T *)r; const T *aa = (const T *)a; const T *bb = (const T *)b; int i;
    if (f) { f(rr, aa, bb, n); return; }
    for (i = 0; i < n; i++) rr[i] = aa[i] > bb[i] ? aa[i] : bb[i];
}

/* --- 归约 (数组→标量, 单趟) --- */

/* hsum: 全数组求和 */
model (T) T mt_mat_hsum(const void *a, int n) {
    switch (mt_packet_lanes(T)()) {
        case 4: return (T)mt_sse_hsum_f_arr((const float *)a, n);
        case 2: return (T)mt_sse_hsum_d_arr((const double *)a, n);
        default: { const T *aa = (const T *)a; T s = 0; int i;
                   for (i = 0; i < n; i++) s += aa[i]; return s; }
    }
}
/* hdot: 全数组点积 */
model (T) T mt_mat_hdot(const void *a, const void *b, int n) {
    switch (mt_packet_lanes(T)()) {
        case 4: return (T)mt_sse_hdot_f_arr((const float *)a, (const float *)b, n);
        case 2: return (T)mt_sse_hdot_d_arr((const double *)a, (const double *)b, n);
        default: { const T *aa = (const T *)a; const T *bb = (const T *)b;
                   T s = 0; int i; for (i = 0; i < n; i++) s += aa[i] * bb[i]; return s; }
    }
}
/* hmin: 全数组最小值 */
model (T) T mt_mat_hmin(const void *a, int n) {
    switch (mt_packet_lanes(T)()) {
        case 4: return (T)mt_sse_hmin_f_arr((const float *)a, n);
        case 2: return (T)mt_sse_hmin_d_arr((const double *)a, n);
        default: { const T *aa = (const T *)a; T v = aa[0]; int i;
                   for (i = 1; i < n; i++) if (aa[i] < v) v = aa[i]; return v; }
    }
}
/* hmax: 全数组最大值 */
model (T) T mt_mat_hmax(const void *a, int n) {
    switch (mt_packet_lanes(T)()) {
        case 4: return (T)mt_sse_hmax_f_arr((const float *)a, n);
        case 2: return (T)mt_sse_hmax_d_arr((const double *)a, n);
        default: { const T *aa = (const T *)a; T v = aa[0]; int i;
                   for (i = 1; i < n; i++) if (aa[i] > v) v = aa[i]; return v; }
    }
}

/* --- GEMM: C(R×N) = A(R×K) · B(K×N) (M5, §10) ---
 * C 行就地累加,要求 C 与 A/B 互不重叠(别名保护在 ops.h mt_mat_prod)。分派 float→
 * SSE F4(缓存分块微核 mt_sse_gemm_b_f), double→SSE D2(mt_sse_gemm_b_d), 其它→标量
 * 三循环。分块微核快路径只在对齐且 N%lanes==0 且 R>MR 时生效,否则其后端内部回退到
 * 基础 rank-1 微核/朴素标量 —— 正确性与 `model` 逐元素语义一致(小矩阵平铺反慢)。 */
model (T) void mt_mat_gemm(void *c, const void *a, const void *b,
                           int R, int K, int N) {
    void (*f)(void *, const void *, const void *, int, int, int) =
        _Generic((T)0, float: mt_sse_gemm_b_f, double: mt_sse_gemm_b_d, default: 0);
    if (f) { f(c, a, b, R, K, N); return; }
    const T *A = (const T *)a, *B = (const T *)b; T *C = (T *)c;
    int i, j, k;
    for (i = 0; i < R; i++)
        for (j = 0; j < N; j++) {
            T s = 0;
            for (k = 0; k < K; k++) s += A[i * K + k] * B[k * N + j];
            C[i * N + j] = s;
        }
}

#endif /* MT_MAT_PACKET_H */