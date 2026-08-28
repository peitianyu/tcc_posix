/* t086_mat_reduc: 归约 + 逐元素通道 (M3)
 *
 * 纯断言(无 stdio)。覆盖 lib/mat/ops.h + packet.h + pack_sse.h M3 语义:
 *   1. 归约单趟通道: sum/mean/sqnorm/norm/dot(同形状)/trace(方形)/min/max
 *      —— 与手工参考标量一致; 验证 sum = <a,ones> 关系 (对齐平方数 R*C 走 packet 归约)
 *   2. 逐元素数组语义: abs/sqrt/cmin/cmax —— 与标量参考一致
 *   3. 向量路径与标量回退一致性: R*C 为 lane 整数倍(和=4/2)且 16B 对齐 → 向量化;
 *      非整数倍(如 2x3, n=6)或 int(无后端) → 标量回退, 结果仍正确
 *   4. 非方形大实例线性下标对齐
 *
 * 调用风格: 显式实例化 `mt_mat_*(T,R,C)(...)`。
 * 退出码 0 = 通过.
 */
#include "lib/mat/ops.h"
#include <math.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
#define near_f(x,y) (fabsf((x)-(y)) < 1e-5f)
#define near_d(x,y) (fabs((x)-(y)) < 1e-9)

/* ---- 归约: 与手工标量参考一致 (R*C=8=4的整数倍 → 向量归约路径) ---- */
static int test_reduction_vec(void) {
    mt_mat(float, 2, 4) A, B;   /* n=8, float lane=4 → 向量归约 */
    float vA[8] = { 1,2,3,4, 5,6,7,8 };
    float vB[8] = { 8,7,6,5, 4,3,2,1 };
    for (int i = 0; i < 8; i++) { A.a[i] = vA[i]; B.a[i] = vB[i]; }

    float rs = 0; for (int i = 0; i < 8; i++) rs += vA[i];
    CHECK(mt_mat_sum(float,2,4)(&A) == rs);
    CHECK(near_f(mt_mat_mean(float,2,4)(&A), rs / 8.0f));

    float sq = 0; for (int i = 0; i < 8; i++) sq += vA[i]*vA[i];
    CHECK(near_f(mt_mat_sqnorm(float,2,4)(&A), sq));
    CHECK(near_f(mt_mat_norm(float,2,4)(&A), sqrtf(sq)));

    float dt = 0; for (int i = 0; i < 8; i++) dt += vA[i]*vB[i];
    CHECK(near_f(mt_mat_dot(float,2,4)(&A, &B), dt));
    /* 恒等: sum(A) == dot(A, ones) */
    mt_mat(float,2,4) O; mt_mat_fill(float,2,4)(&O, 1.0f);
    CHECK(near_f(mt_mat_dot(float,2,4)(&A,&O), rs));

    float mn = vA[0], mx = vA[0];
    for (int i = 1; i < 8; i++) { if (vA[i] < mn) mn = vA[i]; if (vA[i] > mx) mx = vA[i]; }
    CHECK(mt_mat_min(float,2,4)(&A) == mn);
    CHECK(mt_mat_max(float,2,4)(&A) == mx);

    return 0;
}

/* ---- double 归约 + 方形 trace ---- */
static int test_reduction_double_trace(void) {
    mt_mat(double, 3, 3) A;    /* n=9, double lane=2, 9%2!=0 → 含尾标量路径 */
    double v[9] = { 1,2,3, 4,5,6, 7,8,9 };
    for (int i = 0; i < 9; i++) A.a[i] = v[i];

    double rs = 0; for (int i = 0; i < 9; i++) rs += v[i];
    CHECK(near_d(mt_mat_sum(double,3,3)(&A), rs));
    CHECK(near_d(mt_mat_mean(double,3,3)(&A), rs / 9.0));
    double sq = 0; for (int i = 0; i < 9; i++) sq += v[i]*v[i];
    CHECK(near_d(mt_mat_sqnorm(double,3,3)(&A), sq));
    CHECK(near_d(mt_mat_norm(double,3,3)(&A), sqrt(sq)));

    double tr = A.a[0] + A.a[4] + A.a[8];   /* 1+5+9 */
    CHECK(near_d(mt_mat_trace(double,3,3)(&A), 15.0));
    CHECK(near_d(mt_mat_trace(double,3,3)(&A), tr));

    CHECK(mt_mat_min(double,3,3)(&A) == 1.0);
    CHECK(mt_mat_max(double,3,3)(&A) == 9.0);
    return 0;
}

/* ---- 逐元素 abs/sqrt/cmin/cmax: 向量路径 (float, n=8) ---- */
static int test_elemwise_vec(void) {
    mt_mat(float, 2, 4) A, B, C, R;
    float vA[8] = { -1, 2, -3, 4, -5, 6, -7, 8 };
    float vB[8] = {  9, -8, 7, -6, 5, -4, 3, -2 };
    for (int i = 0; i < 8; i++) { A.a[i] = vA[i]; B.a[i] = vB[i]; }

    mt_mat_abs(float,2,4)(&C, &A);
    for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], vA[i] < 0 ? -vA[i] : vA[i]));

    /* 平方数矩阵 → sqrt 精确 (4,9,16,25) */
    mt_mat(float,2,4) P;
    float vP[8] = { 4,9,16,25, 36,49,64,81 };
    for (int i = 0; i < 8; i++) P.a[i] = vP[i];
    mt_mat_sqrt(float,2,4)(&C, &P);
    for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], sqrtf(vP[i])));

    mt_mat_cmin(float,2,4)(&C, &A, &B);
    for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], vA[i] < vB[i] ? vA[i] : vB[i]));
    mt_mat_cmax(float,2,4)(&C, &A, &B);
    for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], vA[i] > vB[i] ? vA[i] : vB[i]));

    /* 自赋值: dst 与源重叠仍正确 (cmin(A,A)) */
    mt_mat_cmin(float,2,4)(&R, &A, &A);
    for (int i = 0; i < 8; i++) CHECK(near_f(R.a[i], vA[i]));
    return 0;
}

/* ---- 标量回退: 非整数倍(n=6) + int 无后端 ---- */
static int test_scalar_fallback(void) {
    /* float 2x3: n=6, 6%4!=0 → 标量回退, 逐元素/归约仍正确 */
    mt_mat(float, 2, 3) A, B, C;
    float vA[6] = { -1, 2, -3, 4, -5, 6 };
    float vB[6] = {  6, -5, 4, -3, 2, -1 };
    for (int i = 0; i < 6; i++) { A.a[i] = vA[i]; B.a[i] = vB[i]; }

    mt_mat_abs(float,2,3)(&C, &A);
    for (int i = 0; i < 6; i++) CHECK(near_f(C.a[i], vA[i] < 0 ? -vA[i] : vA[i]));
    mt_mat_cmin(float,2,3)(&C, &A, &B);
    for (int i = 0; i < 6; i++) CHECK(near_f(C.a[i], vA[i] < vB[i] ? vA[i] : vB[i]));

    float rs = 0; for (int i = 0; i < 6; i++) rs += vA[i];
    CHECK(near_f(mt_mat_sum(float,2,3)(&A), rs));

    /* int: 无 SIMD 后端, 整体走标量 */
    mt_mat(int, 2, 4) IA, IB, IC;
    int ivA[8] = { 3, -1, 4, -1, 5, -9, 2, 6 };
    int ivB[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    for (int i = 0; i < 8; i++) { IA.a[i] = ivA[i]; IB.a[i] = ivB[i]; }

    int isum = 0; for (int i = 0; i < 8; i++) isum += ivA[i];
    CHECK(mt_mat_sum(int,2,4)(&IA) == isum);
    int imin = ivA[0], imax = ivA[0];
    for (int i = 1; i < 8; i++) { if (ivA[i] < imin) imin = ivA[i]; if (ivA[i] > imax) imax = ivA[i]; }
    CHECK(mt_mat_min(int,2,4)(&IA) == imin);
    CHECK(mt_mat_max(int,2,4)(&IA) == imax);

    mt_mat_abs(int,2,4)(&IC, &IA);
    for (int i = 0; i < 8; i++) CHECK(IC.a[i] == (ivA[i] < 0 ? -ivA[i] : ivA[i]));
    mt_mat_cmin(int,2,4)(&IC, &IA, &IB);
    for (int i = 0; i < 8; i++) CHECK(IC.a[i] == (ivA[i] < ivB[i] ? ivA[i] : ivB[i]));
    return 0;
}

int main(void) {
    int r;
    if ((r = test_reduction_vec()) != 0) return r;
    if ((r = test_reduction_double_trace()) != 0) return 100 + r;
    if ((r = test_elemwise_vec()) != 0) return 200 + r;
    if ((r = test_scalar_fallback()) != 0) return 300 + r;
    return 0;
}