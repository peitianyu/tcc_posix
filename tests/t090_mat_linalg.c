/* t090_mat_linalg: 直接法 LU/行列式/逆 + Cholesky (M6)
 *
 * 纯断言(无 stdio)。覆盖 lib/mat/linalg.h 语义(见 docs/matrix.md §1.2 分解1/2):
 *   1. LU 分解: 交给 mt_lu_factor, 由 P A = L U 重构校验(含部分选主元/符号位)
 *   2. 行列式: 已知方阵的手算 det 精确比对(float/double)
 *   3. 逆矩阵: A^{-1} 与 A 乘积重建单位阵; 奇异矩阵返回 1
 *   4. Cholesky: A = L L^T 重构校验; 非正定矩阵返回 1
 *
 * 调用风格: 显式实例化 `mt_mat_*(T,R,C)(...)`。
 * 退出码 0 = 通过.
 */
#include "lib/mat/linalg.h"
#include <math.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
#define near_f(x,y) (fabsf((x)-(y)) < 1e-4f)
#define near_d(x,y) (fabs((x)-(y)) < 1e-10)

/* ---- 1. LU: 3x3 部分选主元, P A = L U 重构 ---- */
static int test_lu_float(void) {
    mt_mat(float, 3, 3) A, w, P, L, U;
    /* 取需要选主元/换行的矩阵 */
    A.a[0]=2;  A.a[1]=1; A.a[2]=-1;
    A.a[3]=-3; A.a[4]=-1; A.a[5]=2;
    A.a[6]=-2; A.a[7]=1; A.a[8]=2;
    int piv[3], sign, i, j, k;
    for (i = 0; i < 9; i++) w.a[i] = A.a[i];
    CHECK(mt_lu_factor(float, 3)(&w, piv, &sign) == 0);
    /* P[i][piv[i]] = 1 */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) P.a[i*3+j] = (piv[i] == j) ? 1.0f : 0.0f;
    /* L(单位下三角) U(上三角) 自合并存储析出 */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            L.a[i*3+j] = (i > j) ? w.a[i*3+j] : (i == j ? 1.0f : 0.0f);
            U.a[i*3+j] = (i <= j) ? w.a[i*3+j] : 0.0f;
        }
    /* L·U vs P·A */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            float lu = 0, pa = 0;
            for (k = 0; k < 3; k++) { lu += L.a[i*3+k]*U.a[k*3+j]; pa += P.a[i*3+k]*A.a[k*3+j]; }
            CHECK(near_f(lu, pa));
        }
    return 0;
}

/* ---- 2. 行列式已知值: det = 9 (A=[[2,0,1],[1,3,2],[1,0,2]]) ---- */
static int test_det(void) {
    mt_mat(double, 3, 3) A;
    A.a[0]=2; A.a[1]=0; A.a[2]=1;
    A.a[3]=1; A.a[4]=3; A.a[5]=2;
    A.a[6]=1; A.a[7]=0; A.a[8]=2;
    CHECK(near_d(mt_mat_det(double, 3)(&A), 9.0));
    /* float 版同一矩阵 */
    mt_mat(float, 3, 3) B;
    for (int i = 0; i < 9; i++) B.a[i] = (float)A.a[i];
    CHECK(near_f(mt_mat_det(float, 3)(&B), 9.0f));
    /* 单位阵 det = 1 */
    mt_mat(float, 3, 3) I; mt_mat_identity(float, 3, 3)(&I);
    CHECK(near_f(mt_mat_det(float, 3)(&I), 1.0f));
    return 0;
}

/* ---- 3. 逆矩阵: A·A^{-1} = I, 奇异返回 1 ---- */
static int test_inverse(void) {
    /* 非奇异 3x3 */
    mt_mat(double, 3, 3) A, inv, I;
    A.a[0]=2; A.a[1]=1; A.a[2]=-1;
    A.a[3]=-3; A.a[4]=-1; A.a[5]=2;
    A.a[6]=-2; A.a[7]=1; A.a[8]=2;
    CHECK(mt_mat_inverse(double, 3)(&inv, &A) == 0);
    mt_mat_identity(double, 3, 3)(&I);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            double s = 0;
            for (int k = 0; k < 3; k++) s += A.a[i*3+k]*inv.a[k*3+j];
            CHECK(near_d(s, I.a[i*3+j]));
        }
    /* 就地: inv(A) 与 A 同址 */
    mt_mat(double, 3, 3) B;
    for (int i = 0; i < 9; i++) B.a[i] = A.a[i];
    CHECK(mt_mat_inverse(double, 3)(&B, &B) == 0);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            double s = 0;
            for (int k = 0; k < 3; k++) s += A.a[i*3+k]*B.a[k*3+j];
            CHECK(near_d(s, I.a[i*3+j]));
        }
    /* 奇异矩阵 → 返回 1 */
    mt_mat(double, 3, 3) S, tmp;
    S.a[0]=1; S.a[1]=2; S.a[2]=3;
    S.a[3]=4; S.a[4]=5; S.a[5]=6;
    S.a[6]=4; S.a[7]=5; S.a[8]=6;      /* row2==row1, 消元出精确 0 → 判奇异 */
    CHECK(mt_mat_inverse(double, 3)(&tmp, &S) == 1);
    return 0;
}

/* ---- 4. Cholesky: A = L L^T 重构; 非正定返回 1 ---- */
static int test_cholesky(void) {
    mt_mat(float, 3, 3) A, L0;
    A.a[0]=4; A.a[1]=2; A.a[2]=0;
    A.a[3]=2; A.a[4]=5; A.a[5]=2;
    A.a[6]=0; A.a[7]=2; A.a[8]=6;       /* 对称正定 (L=[[2],[1 2],[0 1 sqrt5]]) */
    for (int i = 0; i < 9; i++) L0.a[i] = A.a[i];
    CHECK(mt_cholesky(float, 3)(&L0) == 0);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float s = 0;                                /* (L L^T)[i][j], L 仅保下三角 */
            for (int k = 0; k <= (i<j?i:j); k++) s += L0.a[i*3+k]*L0.a[j*3+k];
            CHECK(near_f(s, A.a[i*3+j]));
        }
    /* 非正定: 对称但负行列式 [[1,2],[2,1]] det = -3 */
    mt_mat(float, 2, 2) N;
    N.a[0]=1; N.a[1]=2; N.a[2]=2; N.a[3]=1;
    CHECK(mt_cholesky(float, 2)(&N) == 1);
    return 0;
}

int main(void) {
    CHECK(test_lu_float() == 0);
    CHECK(test_det() == 0);
    CHECK(test_inverse() == 0);
    CHECK(test_cholesky() == 0);
    return 0;
}