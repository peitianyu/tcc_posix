/* matrix pack_sse.h - SIMD packet 后端落地 (M2)
 *
 * SSE(128 位 XMM) backend: float→4 lane, double→2 lane。对应 docs/matrix.md §9,
 * 对标 Eigen packet_traits 的后端实现。**算法层不直接依赖本文件**, 一律走 packet.h
 * 的 `mt_mat_vec_*` 语义。
 *
 * 对齐策略: 本后端统一用**对齐 load/store**(`_mm_load_ps`/`_mm_store_ps` 等)。因
 * packet.h 仅当 `n%lanes==0` 且基址 16B 对齐时才向量化(can_vec), 且 `mt_mat` 类型级
 * 已强制 16B 对齐(见 matrix.h), 故可安全用对齐访问。**任一缓冲未 16B 对齐则整段回退
 * 标量**(运行时守卫 mt_sse_ok16), 正确优先于最快。注意: TCC 未注册 `_mm_loadu_ps`/
 * `_mm_set1_ps` 等 loadu/广播名, 故标量广播用对齐小数组 + `_mm_load_ps` 展开(vector
 * 层种子)。
 *
 * 函数签名统一:
 *   binary  `void (void*, const void*, const void*, int)`   // r, a, b, n
 *   scal    `void (void*, const void*, <T>, int)`           // r, a, s, n
 *   sqrt    `void (void*, const void*, int)`                // r, a, n
 * 便于 packet.h 用 _Generic 以统一接口分派 float/double。
 *
 * 局限(docs/matrix.md §14): 仅 float/double; SIMD 宽度即 SSE 硬上限。
 */
#ifndef MT_MAT_PACK_SSE_H
#define MT_MAT_PACK_SSE_H

#include "simd.h"
#include <stddef.h>
#include <math.h>

/* 本层函数改 `static`(非 inline): tcc 的 model 泛型重放会以外部符号引用这些 helper,
 * `static inline` 不产符号 → 链接 unresolved(与 lib/allocator.h 同教训, 见其注释)。
 * 附加 __attribute__((unused)) 抑制未使用告警。 */
#define MT_SSE_STATIC static __attribute__((unused))

/* 16B 对齐检测: 用于运行时守卫, 未对齐则整段标量回退。 */
MT_SSE_STATIC int mt_sse_ok16(const void *p) {
    return (((size_t)p) & 15u) == 0;
}

/* ---------------- float: 4 lane ---------------- */

MT_SSE_STATIC void mt_sse_add_f(void *r, const void *a, const void *b, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a;
    const float *bb = (const float *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 4 <= n; i += 4)
            _mm_store_ps(rr + i, _mm_add_ps(_mm_load_ps(aa + i), _mm_load_ps(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] + bb[i];
}
MT_SSE_STATIC void mt_sse_sub_f(void *r, const void *a, const void *b, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a;
    const float *bb = (const float *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 4 <= n; i += 4)
            _mm_store_ps(rr + i, _mm_sub_ps(_mm_load_ps(aa + i), _mm_load_ps(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] - bb[i];
}
MT_SSE_STATIC void mt_sse_mul_f(void *r, const void *a, const void *b, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a;
    const float *bb = (const float *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 4 <= n; i += 4)
            _mm_store_ps(rr + i, _mm_mul_ps(_mm_load_ps(aa + i), _mm_load_ps(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] * bb[i];
}
MT_SSE_STATIC void mt_sse_scal_f(void *r, const void *a, float s, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a)) {
        _Alignas(16) float s4[4]; int k;
        for (k = 0; k < 4; k++) s4[k] = s;          /* 保守广播种子(无 _mm_set1_ps) */
        __m128 sf = _mm_load_ps(s4);
        for (; i + 4 <= n; i += 4)
            _mm_store_ps(rr + i, _mm_mul_ps(_mm_load_ps(aa + i), sf));
    }
    for (; i < n; i++) rr[i] = aa[i] * s;
}
MT_SSE_STATIC void mt_sse_sqrt_f(void *r, const void *a, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a)) {
        for (; i + 4 <= n; i += 4)
            _mm_store_ps(rr + i, _mm_sqrt_ps(_mm_load_ps(aa + i)));
    }
    for (; i < n; i++) rr[i] = sqrtf(aa[i]);
}

/* ---------------- double: 2 lane ---------------- */

MT_SSE_STATIC void mt_sse_add_d(void *r, const void *a, const void *b, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a;
    const double *bb = (const double *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 2 <= n; i += 2)
            _mm_store_pd(rr + i, _mm_add_pd(_mm_load_pd(aa + i), _mm_load_pd(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] + bb[i];
}
MT_SSE_STATIC void mt_sse_sub_d(void *r, const void *a, const void *b, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a;
    const double *bb = (const double *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 2 <= n; i += 2)
            _mm_store_pd(rr + i, _mm_sub_pd(_mm_load_pd(aa + i), _mm_load_pd(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] - bb[i];
}
MT_SSE_STATIC void mt_sse_mul_d(void *r, const void *a, const void *b, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a;
    const double *bb = (const double *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 2 <= n; i += 2)
            _mm_store_pd(rr + i, _mm_mul_pd(_mm_load_pd(aa + i), _mm_load_pd(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] * bb[i];
}
MT_SSE_STATIC void mt_sse_scal_d(void *r, const void *a, double s, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a)) {
        _Alignas(16) double s2[2]; int k;
        for (k = 0; k < 2; k++) s2[k] = s;          /* 保守广播种子(无 _mm_set1_pd) */
        __m128d sd = _mm_load_pd(s2);
        for (; i + 2 <= n; i += 2)
            _mm_store_pd(rr + i, _mm_mul_pd(_mm_load_pd(aa + i), sd));
    }
    for (; i < n; i++) rr[i] = aa[i] * s;
}
MT_SSE_STATIC void mt_sse_sqrt_d(void *r, const void *a, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a)) {
        for (; i + 2 <= n; i += 2)
            _mm_store_pd(rr + i, _mm_sqrt_pd(_mm_load_pd(aa + i)));
    }
    for (; i < n; i++) rr[i] = sqrt(aa[i]);
}

/* ---------------- abs: |x| = max(x, -x) ---------------- */

MT_SSE_STATIC void mt_sse_abs_f(void *r, const void *a, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a)) {
        __m128 zero = _mm_setzero_ps();
        for (; i + 4 <= n; i += 4) {
            __m128 x = _mm_load_ps(aa + i);
            _mm_store_ps(rr + i, _mm_max_ps(x, _mm_sub_ps(zero, x)));
        }
    }
    for (; i < n; i++) rr[i] = aa[i] < 0 ? -aa[i] : aa[i];
}
MT_SSE_STATIC void mt_sse_abs_d(void *r, const void *a, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a)) {
        __m128d zero = _mm_setzero_pd();
        for (; i + 2 <= n; i += 2) {
            __m128d x = _mm_load_pd(aa + i);
            _mm_store_pd(rr + i, _mm_max_pd(x, _mm_sub_pd(zero, x)));
        }
    }
    for (; i < n; i++) rr[i] = aa[i] < 0 ? -aa[i] : aa[i];
}

/* ---------------- element-wise min/max ---------------- */

MT_SSE_STATIC void mt_sse_min_f(void *r, const void *a, const void *b, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a;
    const float *bb = (const float *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 4 <= n; i += 4)
            _mm_store_ps(rr + i, _mm_min_ps(_mm_load_ps(aa + i), _mm_load_ps(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] < bb[i] ? aa[i] : bb[i];
}
MT_SSE_STATIC void mt_sse_min_d(void *r, const void *a, const void *b, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a;
    const double *bb = (const double *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 2 <= n; i += 2)
            _mm_store_pd(rr + i, _mm_min_pd(_mm_load_pd(aa + i), _mm_load_pd(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] < bb[i] ? aa[i] : bb[i];
}
MT_SSE_STATIC void mt_sse_max_f(void *r, const void *a, const void *b, int n) {
    float *rr = (float *)r; const float *aa = (const float *)a;
    const float *bb = (const float *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 4 <= n; i += 4)
            _mm_store_ps(rr + i, _mm_max_ps(_mm_load_ps(aa + i), _mm_load_ps(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] > bb[i] ? aa[i] : bb[i];
}
MT_SSE_STATIC void mt_sse_max_d(void *r, const void *a, const void *b, int n) {
    double *rr = (double *)r; const double *aa = (const double *)a;
    const double *bb = (const double *)b; int i = 0;
    if (mt_sse_ok16(r) && mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 2 <= n; i += 2)
            _mm_store_pd(rr + i, _mm_max_pd(_mm_load_pd(aa + i), _mm_load_pd(bb + i)));
    }
    for (; i < n; i++) rr[i] = aa[i] > bb[i] ? aa[i] : bb[i];
}

/* ---------------- 水平归约 (数组→标量) ---------------- */

/* hsum: 全数组求和 */
MT_SSE_STATIC float mt_sse_hsum_f_arr(const float *a, int n) {
    __m128 acc = _mm_setzero_ps(); int i = 0;
    if (mt_sse_ok16(a)) {
        for (; i + 4 <= n; i += 4)
            acc = _mm_add_ps(acc, _mm_load_ps(a + i));
    }
    _Alignas(16) float tmp[4];
    _mm_store_ps(tmp, acc);
    float s = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < n; i++) s += a[i];
    return s;
}
MT_SSE_STATIC double mt_sse_hsum_d_arr(const double *a, int n) {
    __m128d acc = _mm_setzero_pd(); int i = 0;
    if (mt_sse_ok16(a)) {
        for (; i + 2 <= n; i += 2)
            acc = _mm_add_pd(acc, _mm_load_pd(a + i));
    }
    _Alignas(16) double tmp[2];
    _mm_store_pd(tmp, acc);
    double s = tmp[0] + tmp[1];
    for (; i < n; i++) s += a[i];
    return s;
}

/* hdot: 全数组点积 (a·b) */
MT_SSE_STATIC float mt_sse_hdot_f_arr(const float *a, const float *b, int n) {
    __m128 acc = _mm_setzero_ps(); int i = 0;
    if (mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 4 <= n; i += 4)
            acc = _mm_add_ps(acc, _mm_mul_ps(_mm_load_ps(a + i), _mm_load_ps(b + i)));
    }
    _Alignas(16) float tmp[4];
    _mm_store_ps(tmp, acc);
    float s = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < n; i++) s += a[i] * b[i];
    return s;
}
MT_SSE_STATIC double mt_sse_hdot_d_arr(const double *a, const double *b, int n) {
    __m128d acc = _mm_setzero_pd(); int i = 0;
    if (mt_sse_ok16(a) && mt_sse_ok16(b)) {
        for (; i + 2 <= n; i += 2)
            acc = _mm_add_pd(acc, _mm_mul_pd(_mm_load_pd(a + i), _mm_load_pd(b + i)));
    }
    _Alignas(16) double tmp[2];
    _mm_store_pd(tmp, acc);
    double s = tmp[0] + tmp[1];
    for (; i < n; i++) s += a[i] * b[i];
    return s;
}

/* hmin: 全数组最小值 */
MT_SSE_STATIC float mt_sse_hmin_f_arr(const float *a, int n) {
    if (n <= 0) return 0;
    int i = 0;
    if (mt_sse_ok16(a) && n >= 4) {
        __m128 acc = _mm_load_ps(a); i = 4;
        for (; i + 4 <= n; i += 4)
            acc = _mm_min_ps(acc, _mm_load_ps(a + i));
        _Alignas(16) float tmp[4];
        _mm_store_ps(tmp, acc);
        float v = tmp[0];
        for (int k = 1; k < 4; k++) if (tmp[k] < v) v = tmp[k];
        for (; i < n; i++) if (a[i] < v) v = a[i];
        return v;
    }
    float v = a[0];
    for (i = 1; i < n; i++) if (a[i] < v) v = a[i];
    return v;
}
MT_SSE_STATIC double mt_sse_hmin_d_arr(const double *a, int n) {
    if (n <= 0) return 0;
    int i = 0;
    if (mt_sse_ok16(a) && n >= 2) {
        __m128d acc = _mm_load_pd(a); i = 2;
        for (; i + 2 <= n; i += 2)
            acc = _mm_min_pd(acc, _mm_load_pd(a + i));
        _Alignas(16) double tmp[2];
        _mm_store_pd(tmp, acc);
        double v = tmp[0];
        if (tmp[1] < v) v = tmp[1];
        for (; i < n; i++) if (a[i] < v) v = a[i];
        return v;
    }
    double v = a[0];
    for (i = 1; i < n; i++) if (a[i] < v) v = a[i];
    return v;
}

/* hmax: 全数组最大值 */
MT_SSE_STATIC float mt_sse_hmax_f_arr(const float *a, int n) {
    if (n <= 0) return 0;
    int i = 0;
    if (mt_sse_ok16(a) && n >= 4) {
        __m128 acc = _mm_load_ps(a); i = 4;
        for (; i + 4 <= n; i += 4)
            acc = _mm_max_ps(acc, _mm_load_ps(a + i));
        _Alignas(16) float tmp[4];
        _mm_store_ps(tmp, acc);
        float v = tmp[0];
        for (int k = 1; k < 4; k++) if (tmp[k] > v) v = tmp[k];
        for (; i < n; i++) if (a[i] > v) v = a[i];
        return v;
    }
    float v = a[0];
    for (i = 1; i < n; i++) if (a[i] > v) v = a[i];
    return v;
}
MT_SSE_STATIC double mt_sse_hmax_d_arr(const double *a, int n) {
    if (n <= 0) return 0;
    int i = 0;
    if (mt_sse_ok16(a) && n >= 2) {
        __m128d acc = _mm_load_pd(a); i = 2;
        for (; i + 2 <= n; i += 2)
            acc = _mm_max_pd(acc, _mm_load_pd(a + i));
        _Alignas(16) double tmp[2];
        _mm_store_pd(tmp, acc);
        double v = tmp[0];
        if (tmp[1] > v) v = tmp[1];
        for (; i < n; i++) if (a[i] > v) v = a[i];
        return v;
    }
    double v = a[0];
    for (i = 1; i < n; i++) if (a[i] > v) v = a[i];
    return v;
}

/* ============== GEMM 微核 (M5, docs/matrix.md §10) ==============
 * C(R×N) = A(R×K) · B(K×N)。SSE 快路径按行主序 **i-k-j rank-1 外积累加**:
 * 每 k,把 A[i][k] 广播为 lane 向量,乘 B 的第 k 行一段,累加到 C 第 i 行对应段。
 * C 行就地读改写(累加),要求 C 与 A/B 不重叠 —— 别名保护由上层 mt_mat_prod 负责。
 * 快路径条件=三基址 16B 对齐 且 N 为 lane 整数倍(行步长对齐,每行起点保持 16B);
 * 否则整段标量回退,语义与朴素 i-j-k 完全一致(正确优先, §10.4)。 */
MT_SSE_STATIC void mt_sse_gemm_f(void *c, const void *av, const void *bv,
                                 int R, int K, int N) {
    const float *A = (const float *)av, *B = (const float *)bv;
    float *C = (float *)c;
    int i, k, j;
    if ((N & 3) == 0 && mt_sse_ok16(c) && mt_sse_ok16(av) && mt_sse_ok16(bv)) {
        _Alignas(16) float bcast[4]; int t;
        for (i = 0; i < R; i++) {
            for (j = 0; j < N; j += 4)
                _mm_store_ps(C + i * N + j, _mm_setzero_ps());
            for (k = 0; k < K; k++) {
                float ak = A[i * K + k];
                for (t = 0; t < 4; t++) bcast[t] = ak;      /* 保守广播(无 _mm_set1_ps) */
                __m128 bv_s = _mm_load_ps(bcast);
                for (j = 0; j < N; j += 4) {
                    __m128 acc = _mm_load_ps(C + i * N + j);
                    _mm_store_ps(C + i * N + j,
                        _mm_add_ps(acc, _mm_mul_ps(_mm_load_ps(B + k * N + j), bv_s)));
                }
            }
        }
        return;
    }
    for (i = 0; i < R; i++)
        for (j = 0; j < N; j++) {
            float s = 0;
            for (k = 0; k < K; k++) s += A[i * K + k] * B[k * N + j];
            C[i * N + j] = s;
        }
}
MT_SSE_STATIC void mt_sse_gemm_d(void *c, const void *av, const void *bv,
                                 int R, int K, int N) {
    const double *A = (const double *)av, *B = (const double *)bv;
    double *C = (double *)c;
    int i, k, j;
    if ((N & 1) == 0 && mt_sse_ok16(c) && mt_sse_ok16(av) && mt_sse_ok16(bv)) {
        _Alignas(16) double bcast[2]; int t;
        for (i = 0; i < R; i++) {
            for (j = 0; j < N; j += 2)
                _mm_store_pd(C + i * N + j, _mm_setzero_pd());
            for (k = 0; k < K; k++) {
                double ak = A[i * K + k];
                for (t = 0; t < 2; t++) bcast[t] = ak;
                __m128d bv_s = _mm_load_pd(bcast);
                for (j = 0; j < N; j += 2) {
                    __m128d acc = _mm_load_pd(C + i * N + j);
                    _mm_store_pd(C + i * N + j,
                        _mm_add_pd(acc, _mm_mul_pd(_mm_load_pd(B + k * N + j), bv_s)));
                }
            }
        }
        return;
    }
    for (i = 0; i < R; i++)
        for (j = 0; j < N; j++) {
            double s = 0;
            for (k = 0; k < K; k++) s += A[i * K + k] * B[k * N + j];
            C[i * N + j] = s;
        }
}

/* ---- 缓存分块 GEMM (docs/matrix.md §10.1/§10.2), rank-1 外积累加微核 ----
 *
 * 相对 mt_sse_gemm_f/d(mr=1) 的三点收益:
 *   1. k-轴分块(KC): 微核只在 KC 连续段内遍历 k, B 的 KC×NR 块短驻 L2, 顺序访问;
 *   2. 行阻塞(MR): 同一 B 段(bv)被 MR 行复用 → B 读量减 MR 倍;
 *   3. C 写阻塞: 每个 MR×NR 累加块只写回一次, 微核期间 C 停留寄存器(compute-bound)。
 *
 * 快路径前提: 三基址 16B 对齐 且 N%lanes==0 且 R>MR(至少两行); 否则整体回退到
 * 基础版 mt_sse_gemm_f/d(标量语义一致)。KC 依 lane 宽选择(float L2 块 > double bytes
 * 等价): 微核 MR×NR 累加器全在局部 vector(N%lane==0 ⇒ 行内偏移天然对齐, 无需 packing)。 */
#define MT_SSE_GEMM_FKC  64
#define MT_SSE_GEMM_DKC  32
MT_SSE_STATIC int mt_sse_gemm_min(int a, int b) { return a < b ? a : b; }
MT_SSE_STATIC void mt_sse_gemm_b_f(void *c, const void *av, const void *bv,
                                   int R, int K, int N) {
    const float *A = (const float *)av, *B = (const float *)bv;
    float *C = (float *)c;
    enum { MR = 4, NR = 4 };
    if (R <= MR || N < NR || (N & 3) != 0
        || !mt_sse_ok16(c) || !mt_sse_ok16(av) || !mt_sse_ok16(bv)) {
        mt_sse_gemm_f(c, av, bv, R, K, N); return;
    }
    int kc0, mc0, nc0, i, j, k;
    for (kc0 = 0; kc0 < K; kc0 += MT_SSE_GEMM_FKC) {
        int kcb = mt_sse_gemm_min(K - kc0, MT_SSE_GEMM_FKC);
        for (mc0 = 0; mc0 < R; mc0 += MR) {
            int mrem = mt_sse_gemm_min(MR, R - mc0);
            for (nc0 = 0; nc0 < N; nc0 += NR) {
                _Alignas(16) float scl[MR];
                _Alignas(16) __m128 acc[MR];
                if (kc0 == 0)
                    for (i = 0; i < mrem; i++) acc[i] = _mm_setzero_ps(); /* 首块清零 */
                else
                    for (i = 0; i < mrem; i++) acc[i] = _mm_load_ps(C + (mc0 + i) * N + nc0); /* 续累加 */
                for (k = 0; k < kcb; k++) {
                    __m128 bv_s = _mm_load_ps(B + (kc0 + k) * N + nc0); /* 对齐: N/Nc0 均 4 倍数 */
                    for (i = 0; i < mrem; i++) {
                        float ael = A[(mc0 + i) * K + (kc0 + k)];
                        for (j = 0; j < MR; j++) scl[j] = ael;      /* 保守广播 */
                        __m128 as = _mm_load_ps(scl);
                        acc[i] = _mm_add_ps(acc[i], _mm_mul_ps(bv_s, as));
                    }
                }
                for (i = 0; i < mrem; i++) _mm_store_ps(C + (mc0 + i) * N + nc0, acc[i]);
            }
        }
    }
}
MT_SSE_STATIC void mt_sse_gemm_b_d(void *c, const void *av, const void *bv,
                                   int R, int K, int N) {
    const double *A = (const double *)av, *B = (const double *)bv;
    double *C = (double *)c;
    enum { MR = 2, NR = 2 };
    if (R <= MR || N < NR || (N & 1) != 0
        || !mt_sse_ok16(c) || !mt_sse_ok16(av) || !mt_sse_ok16(bv)) {
        mt_sse_gemm_d(c, av, bv, R, K, N); return;
    }
    int kc0, mc0, nc0, i, j, k;
    for (kc0 = 0; kc0 < K; kc0 += MT_SSE_GEMM_DKC) {
        int kcb = mt_sse_gemm_min(K - kc0, MT_SSE_GEMM_DKC);
        for (mc0 = 0; mc0 < R; mc0 += MR) {
            int mrem = mt_sse_gemm_min(MR, R - mc0);
            for (nc0 = 0; nc0 < N; nc0 += NR) {
                _Alignas(16) double scl[MR];
                _Alignas(16) __m128d acc[MR];
                if (kc0 == 0)
                    for (i = 0; i < mrem; i++) acc[i] = _mm_setzero_pd();
                else
                    for (i = 0; i < mrem; i++) acc[i] = _mm_load_pd(C + (mc0 + i) * N + nc0);
                for (k = 0; k < kcb; k++) {
                    __m128d bv_s = _mm_load_pd(B + (kc0 + k) * N + nc0);
                    for (i = 0; i < mrem; i++) {
                        double ael = A[(mc0 + i) * K + (kc0 + k)];
                        for (j = 0; j < MR; j++) scl[j] = ael;
                        __m128d as = _mm_load_pd(scl);
                        acc[i] = _mm_add_pd(acc[i], _mm_mul_pd(bv_s, as));
                    }
                }
                for (i = 0; i < mrem; i++) _mm_store_pd(C + (mc0 + i) * N + nc0, acc[i]);
            }
        }
    }
}

#endif /* MT_MAT_PACK_SSE_H */