/* t088_mat_gemm: GEMM 矩阵乘 (M5)
 *
 * 纯断言(无 stdio)。覆盖 lib/mat/ops.h 的 M5 语义(见 docs/matrix.md §10):
 *   1. float 对齐快路径(N%4==0 且 16B 对齐): 与朴素 i-j-k 参考一致(容差内)
 *   2. float 非对齐 N(N%4!=0): 后端自动标量回退, 仍正确
 *   3. double 快路径(N%2==0) 与非对齐 N
 *   4. 无 SIMD 类型(int): 标量兜底
 *   5. 别名保护: 就地方阵 C = C·C(dst 与源重叠)结果正确
 *
 * 调用风格: 显式实例化 `mt_mat_*(T,R,C)(...)`。
 * 退出码 0 = 通过.
 */
#include "lib/mat/ops.h"
#include <math.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
#define near_f(x,y) (fabsf((x)-(y)) < 1e-4f)
#define near_d(x,y) (fabs((x)-(y)) < 1e-9)

/* 朴素参考: C(R×N) = A(R×K)·B(K×N) */
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

/* ---- 1. float 对齐快路径 2×3·3×4 → 2×4 ---- */
static int test_float_aligned(void) {
    mt_mat(float, 2, 3) A; mt_mat(float, 3, 4) B; mt_mat(float, 2, 4) D;
    for (int i = 0; i < 6; i++) A.a[i] = (float)(i + 1);
    for (int i = 0; i < 12; i++) B.a[i] = (float)((i * 3) % 7);
    (void) mt_mat_prod(float, 2, 3, 4)(&D, &A, &B);
    float ref[8];
    ref_mul_float(ref, A.a, B.a, 2, 3, 4);
    for (int i = 0; i < 8; i++) CHECK(near_f(D.a[i], ref[i]));
    /* 手工验证一个元素: C[1][3] = Σ A[1][k]*B[k][3] */
    float s = 0; for (int k = 0; k < 3; k++) s += A.a[1 * 3 + k] * B.a[k * 4 + 3];
    CHECK(near_f(D.a[1 * 4 + 3], s));
    return 0;
}

/* ---- 2. float 非对齐 N=5: 后端标量回退, 仍正确 ---- */
static int test_float_unaligned_n(void) {
    mt_mat(float, 2, 3) A; mt_mat(float, 3, 5) B; mt_mat(float, 2, 5) D;
    for (int i = 0; i < 6; i++) A.a[i] = (float)(i + 1);
    for (int i = 0; i < 15; i++) B.a[i] = (float)((i * 5) % 11);
    (void) mt_mat_prod(float, 2, 3, 5)(&D, &A, &B);
    float ref[10];
    ref_mul_float(ref, A.a, B.a, 2, 3, 5);
    for (int i = 0; i < 10; i++) CHECK(near_f(D.a[i], ref[i]));
    return 0;
}

/* ---- 3. double 快路径 2×2·2×2 + 非对齐 N=3 ---- */
static int test_double(void) {
    mt_mat(double, 2, 2) A; mt_mat(double, 2, 2) B; mt_mat(double, 2, 2) D;
    for (int i = 0; i < 4; i++) { A.a[i] = i + 0.5; B.a[i] = (i + 1) * 2.0; }
    (void) mt_mat_prod(double, 2, 2, 2)(&D, &A, &B);
    double ref[4];
    ref_mul_double(ref, A.a, B.a, 2, 2, 2);
    for (int i = 0; i < 4; i++) CHECK(near_d(D.a[i], ref[i]));

    /* 非对齐 N=3 */
    mt_mat(double, 2, 2) A2; mt_mat(double, 2, 3) B2; mt_mat(double, 2, 3) D2;
    for (int i = 0; i < 4; i++) A2.a[i] = i + 1.0;
    for (int i = 0; i < 6; i++) B2.a[i] = (i + 1) * 0.5;
    (void) mt_mat_prod(double, 2, 2, 3)(&D2, &A2, &B2);
    double ref2[6];
    ref_mul_double(ref2, A2.a, B2.a, 2, 2, 3);
    for (int i = 0; i < 6; i++) CHECK(near_d(D2.a[i], ref2[i]));
    return 0;
}

/* ---- 4. 无 SIMD 类型 int: 标量兜底, 精确相等 ---- */
static int test_int_scalar(void) {
    mt_mat(int, 2, 3) A; mt_mat(int, 3, 2) B; mt_mat(int, 2, 2) D;
    int va[6] = { 1,2,3, 4,5,6 };
    int vb[6] = { 7,8, 9,10, 11,12 };
    for (int i = 0; i < 6; i++) { A.a[i] = va[i]; B.a[i] = vb[i]; }
    (void) mt_mat_prod(int, 2, 3, 2)(&D, &A, &B);
    CHECK(D.a[0] == 1*7 + 2*9 + 3*11);
    CHECK(D.a[1] == 1*8 + 2*10 + 3*12);
    CHECK(D.a[2] == 4*7 + 5*9 + 6*11);
    CHECK(D.a[3] == 4*8 + 5*10 + 6*12);
    return 0;
}

/* ---- 5. 别名保护: 就地方阵 D = D·D (dst 与 a/b 同一地址) ---- */
static int test_alias_inplace(void) {
    mt_mat(float, 2, 2) A, R;
    float va[4] = { 1,2, 3,4 };
    for (int i = 0; i < 4; i++) A.a[i] = va[i];
    /* 参考: 先算 M = A·A 到 R */
    (void) mt_mat_prod(float, 2, 2, 2)(&R, &A, &A);
    /* 就地: 把 A 当 dst 再乘 */
    (void) mt_mat_prod(float, 2, 2, 2)(&A, &A, &A);
    for (int i = 0; i < 4; i++) CHECK(near_f(A.a[i], R.a[i]));
    /* 手工: M[0][0] = 1*1+2*3 = 7; M[0][1]=10; M[1][0]=15; M[1][1]=22 */
    CHECK(near_f(A.a[0], 7.0f));
    CHECK(near_f(A.a[1], 10.0f));
    CHECK(near_f(A.a[2], 15.0f));
    CHECK(near_f(A.a[3], 22.0f));
    return 0;
}

int main(void) {
    int r;
    if ((r = test_float_aligned()) != 0) return r;
    if ((r = test_float_unaligned_n()) != 0) return 100 + r;
    if ((r = test_double()) != 0) return 200 + r;
    if ((r = test_int_scalar()) != 0) return 300 + r;
    if ((r = test_alias_inplace()) != 0) return 400 + r;
    return 0;
}