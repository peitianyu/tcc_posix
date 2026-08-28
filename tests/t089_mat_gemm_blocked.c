/* t089_mat_gemm_blocked: GEMM 缓存分块微核 (M5)
 *
 * 纯断言(无 stdio)。覆盖 lib/mat/packet.h M5 分块路径(见 docs/matrix.md §10.1/§10.2):
 *   1. 大 float 交叉跨多个 KC k 分块块(K>64), R 触发 MR 阻塞 → 与朴素参考一致(容差)
 *   2. R%MR 非 0(R=6): 微核尾部行 mrem 正确
 *   3. K%KC 非 0(K=130): 最后一个 k 分块尾部块 kcb 正确
 *   4. double 大尺寸跨块
 *   5. N 非 lane 整数倍(float 非 4 倍数): 分块快路径回退 → 基础/标量仍正确
 *   6. 混合: 分块版本不经 mt_mat_prod,直接调 mt_mat_gemm(校验后端选择正确性)
 *
 * 退出码 0 = 通过.
 */
#include "lib/mat/ops.h"
#include <math.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
#define near_f(x,y) (fabsf((x)-(y)) < 2e-3f)
#define near_d(x,y) (fabs((x)-(y)) < 1e-8)

/* 朴素参考 */
static void ref_mul_float(float *C, const float *A, const float *B,
                          int R, int K, int N) {
    int i, j, k;
    for (i = 0; i < R; i++)
        for (j = 0; j < N; j++) {
            float s = 0;
            for (k = 0; k < K; k++) s += A[i * K + k] * B[k * N + j];
            C[i * N + j] = s;
        }
}
static void ref_mul_double(double *C, const double *A, const double *B,
                           int R, int K, int N) {
    int i, j, k;
    for (i = 0; i < R; i++)
        for (j = 0; j < N; j++) {
            double s = 0;
            for (k = 0; k < K; k++) s += A[i * K + k] * B[k * N + j];
            C[i * N + j] = s;
        }
}

/* 用任意 R×K / K×N GEMM 做校验(栈上扁平数组, 不经 mt_mat 类型形状约束, 便于任意尺寸) */
static int check_float(int R, int K, int N, float tol) {
    enum { MAX = 8192 };
    static float A[MAX], B[MAX], D[MAX], Ref[MAX];
    if (R * K > MAX || K * N > MAX || R * N > MAX) return __LINE__;
    int i;
    for (i = 0; i < R * K; i++) A[i] = (float)((i * 37 + 11) % 101) * 0.25f + 1.0f;
    for (i = 0; i < K * N; i++) B[i] = (float)((i * 53 + 7) % 97) * 0.125f - 3.0f;
    (void) mt_mat_gemm(float)(D, A, B, R, K, N);       /* 走分块微核 */
    ref_mul_float(Ref, A, B, R, K, N);
    for (i = 0; i < R * N; i++) {
        float d = D[i] - Ref[i];
        if (d < 0) d = -d;
        if (d > tol) return 1000 + i;
    }
    return 0;
}
static int check_double(int R, int K, int N, double tol) {
    enum { MAX = 8192 };
    static double A[MAX], B[MAX], D[MAX], Ref[MAX];
    if (R * K > MAX || K * N > MAX || R * N > MAX) return __LINE__;
    int i;
    for (i = 0; i < R * K; i++) A[i] = (double)((i * 37 + 11) % 101) * 0.25 + 1.0;
    for (i = 0; i < K * N; i++) B[i] = (double)((i * 53 + 7) % 97) * 0.125 - 3.0;
    (void) mt_mat_gemm(double)(D, A, B, R, K, N);
    ref_mul_double(Ref, A, B, R, K, N);
    for (i = 0; i < R * N; i++) {
        double d = D[i] - Ref[i];
        if (d < 0) d = -d;
        if (d > tol) return 2000 + i;
    }
    return 0;
}

/* ---- 1/2/3. 大 float 跨多 KC 块 + R%4 + K%KC 尾部 ---- */
static int test_float_blocked(void) {
    int r;
    /* 大而规整: R=16%, K=200(>3 个 KC=64 块), N=32 → 全对齐快路径 */
    if ((r = check_float(16, 200, 32, 2e-3f)) != 0) return r;
    /* R%4=2: R=6 触发 MR 尾行 */
    if ((r = check_float(6, 128, 40, 2e-3f)) != 0) return r + 100;
    /* K%KC=130: 128/64=2 余 2 → 最后一枚 k 块 kcb=2 */
    if ((r = check_float(20, 130, 16, 2e-3f)) != 0) return r + 200;
    /* 单 KC 块内恰好整除 */
    if ((r = check_float(8, 64, 16, 2e-3f)) != 0) return r + 300;
    return 0;
}

/* ---- 4. double 大尺寸跨块 ---- */
static int test_double_blocked(void) {
    int r;
    if ((r = check_double(10, 130, 40, 1e-8)) != 0) return r;
    if ((r = check_double(3, 65, 20, 1e-8)) != 0) return r + 10;   /* R<TMR? R=3>2 触发 */
    if ((r = check_double(2, 66, 22, 1e-8)) != 0) return r + 20;   /* R=MR 边界 → 回退基础版 */
    return 0;
}

/* ---- 5. N 非 lane 倍数: 分块回退(小矩阵)仍正确 ---- */
static int test_unaligned_fallback(void) {
    int r;
    /* N=33 float 非 4 倍数 → mt_sse_gemm_b_f 回退到 f(基础 rank-1/标量) */
    if ((r = check_float(12, 20, 33, 2e-3f)) != 0) return r;
    if ((r = check_float(4, 20, 7, 2e-3f)) != 0) return r + 10;     /* N 小且非倍数 */
    if ((r = check_double(8, 20, 7, 1e-8)) != 0) return r + 20;     /* N=7 double 非 2 倍 */
    return 0;
}

/* ---- 6. 经 mt_mat_prod 全栈(含别名保护), 大矩阵 ---- */
static int test_prod_integration(void) {
    mt_mat(float, 12, 20) A;                                       /* 12×20 */
    mt_mat(float, 20, 32) B;                                       /* 20×32 */
    mt_mat(float, 12, 32) D, Ref;                                  /* 12×32 */
    int i;
    for (i = 0; i < 12 * 20; i++) A.a[i] = (float)((i * 3) % 17) - 8.0f;
    for (i = 0; i < 20 * 32; i++) B.a[i] = (float)((i * 5) % 23) * 0.5f;
    (void) mt_mat_prod(float, 12, 20, 32)(&D, &A, &B);
    ref_mul_float(Ref.a, A.a, B.a, 12, 20, 32);
    for (i = 0; i < 12 * 32; i++) CHECK(near_f(D.a[i], Ref.a[i]));
    return 0;
}

/* test_prod_integration 的就地自乘改用方规模拟,避免形状误用 */
static int test_prod_inplace(void) {
    mt_mat(float, 8, 8) A, Ref;
    int i;
    for (i = 0; i < 64; i++) A.a[i] = (float)((i * 7) % 29) * 0.25f;
    /* 参考 M = A·A */
    ref_mul_float(Ref.a, A.a, A.a, 8, 8, 8);
    (void) mt_mat_prod(float, 8, 8, 8)(&A, &A, &A);                 /* 就地: dst 与 a/b 同址 */
    for (i = 0; i < 64; i++) CHECK(near_f(A.a[i], Ref.a[i]));
    return 0;
}

/* ---- 7. 部分重叠: dst 仅与 a(或仅与 b)重叠, 另一源独立 (08-28 修复回归) ----
 * 旧实现把未重叠源误换成全 0 占位缓冲 → 乘错。这里用同一底层缓冲构造两个 4×4
 * 的 dst 与 A 区间重叠(元素 4..15), 而 B 独立, 精确命中 overlap_a && !overlap_b;
 * 反向构造 dst 与 B 重叠、A 独立, 命中 !overlap_a && overlap_b。 */
static int test_prod_partial_overlap(void) {
    enum { N4 = 4 };
    _Alignas(16) static double raw[32];           /* 元素 0..15 = dst, 4..19 = 重叠矩阵 */
    mt_mat(double, N4, N4) Be;                    /* 独立 B (局部, 16B 对齐) */
    mt_mat(double, N4, N4) Ref;
    int i;
    for (i = 0; i < 32; i++) raw[i] = (double)((i * 13) % 19) * 0.5 - 2.0;
    for (i = 0; i < 16; i++) Be.a[i] = (double)((i * 7) % 11) * 0.25 + 0.5;

    /* 仅 A 与 dst 重叠: A 指向 raw+4 (元素 4..19), dst=raw (0..15); B=Be 独立 */
    mt_mat(double, N4, N4) *dstA = (mt_mat(double, N4, N4) *)raw;
    mt_mat(double, N4, N4) *Aov  = (mt_mat(double, N4, N4) *)(raw + N4);
    ref_mul_double(Ref.a, Aov->a, Be.a, N4, N4, N4);
    (void) mt_mat_prod(double, N4, N4, N4)(dstA, Aov, &Be);
    for (i = 0; i < 16; i++) CHECK(near_d(dstA->a[i], Ref.a[i]));

    /* 仅 B 与 dst 重叠: 重置 dst 区; B 指向 raw+4, A=Be 独立 */
    mt_mat(double, N4, N4) *dstB = (mt_mat(double, N4, N4) *)raw;
    mt_mat(double, N4, N4) *Bov  = (mt_mat(double, N4, N4) *)(raw + N4);
    for (i = 0; i < 16; i++) raw[i] = (double)((i * 31) % 23) * 0.5;
    for (i = 0; i < 16; i++) Be.a[i] = (double)((i * 3) % 17) * 0.25 - 1.0;
    ref_mul_double(Ref.a, Be.a, Bov->a, N4, N4, N4);
    (void) mt_mat_prod(double, N4, N4, N4)(dstB, &Be, Bov);
    for (i = 0; i < 16; i++) CHECK(near_d(dstB->a[i], Ref.a[i]));

    return 0;
}

int main(void) {
    int r;
    if ((r = test_float_blocked()) != 0) return r;
    if ((r = test_double_blocked()) != 0) return 100 + r;
    if ((r = test_unaligned_fallback()) != 0) return 200 + r;
    if ((r = test_prod_integration()) != 0) return 300 + r;
    if ((r = test_prod_inplace()) != 0) return 400 + r;
    if ((r = test_prod_partial_overlap()) != 0) return 500 + r;
    return 0;
}