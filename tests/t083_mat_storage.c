/* t083_mat_storage: model mt_mat(T,R,C) 存储层 (M0)
 *
 * 纯断言(无 stdio)。覆盖(对齐 lib/mat/matrix.h M0 语义):
 *   1. 尺寸: 数组元素数 = R*C, sizeof(a) = R*C*sizeof(T), 类型级 16B 对齐(_Alignof)
 *   2. 构造: fill / zero / identity(方形主对角)
 *   3. 访问: at/set/at_ptr 行主序下标
 *   4. 行列存取: set_row / get_row / set_col
 *   5. 转置: 矩形 R×C → C×R
 *   6. 尺寸 getter: rows / cols / size (编译期常量)
 *
 * 调用风格: 显式实例化 `mt_mat_*(T, R, C)(...)`。
 * 退出码 0 = 通过.
 */
#include "lib/mat/matrix.h"
#include <stdint.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    /* ---- 1. 尺寸 / 对齐 ---- */
    mt_mat(float, 3, 2) m;
    /* 元素数 = R*C; 数据区大小 = R*C*sizeof(T) (对齐 padding 在 union 尾, 不含于 a) */
    if (sizeof(m.a) != 3 * 2 * sizeof(float)) return 1;
    /* 类型级 16B 对齐(声明保证) */
    if (_Alignof(mt_mat(float,3,2)) != 16) return 3;
    if (_Alignof(mt_mat(double,4,4)) != 16) return 4;
    if (_Alignof(mt_mat(int,1,1)) != 16) return 5;

    /* ---- 2. 构造 ---- */
    mt_mat_zero(float, 3, 2)(&m);
    if (mt_mat_at(float, 3, 2)(&m, 2, 1) != 0.0f) return 10;
    mt_mat_fill(float, 3, 2)(&m, 7.5f);
    if (mt_mat_at(float, 3, 2)(&m, 0, 0) != 7.5f) return 11;
    if (mt_mat_at(float, 3, 2)(&m, 2, 1) != 7.5f) return 12;

    mt_mat(int, 3, 3) I;
    mt_mat_identity(int, 3, 3)(&I);
    if (mt_mat_at(int, 3, 3)(&I, 0, 0) != 1) return 13;
    if (mt_mat_at(int, 3, 3)(&I, 1, 1) != 1) return 14;
    if (mt_mat_at(int, 3, 3)(&I, 2, 2) != 1) return 15;
    if (mt_mat_at(int, 3, 3)(&I, 0, 1) != 0) return 16;
    if (mt_mat_at(int, 3, 3)(&I, 2, 0) != 0) return 17;
    if (mt_mat_at(int, 3, 3)(&I, 1, 2) != 0) return 18;

    /* ---- 3. 访问 / 行主序 ---- */
    mt_mat(float, 2, 3) m2;
    mt_mat_zero(float, 2, 3)(&m2);
    mt_mat_set(float, 2, 3)(&m2, 1, 2, 42.0f);           /* a[1*3+2]=a[5] */
    if (m2.a[5] != 42.0f) return 20;
    if (mt_mat_at(float, 2, 3)(&m2, 1, 2) != 42.0f) return 21;
    *mt_mat_at_ptr(float, 2, 3)(&m2, 0, 1) = -3.0f;       /* a[1] */
    if (mt_mat_at(float, 2, 3)(&m2, 0, 1) != -3.0f) return 22;
    if (mt_mat_cptr(float, 2, 3)(&m2) != m2.a) return 23;

    /* ---- 4. 行列存取 ---- */
    mt_mat(double, 3, 2) md;
    double row[2] = {1.5, 2.5};
    double col[3] = {10.0, 20.0, 30.0};
    mt_mat_zero(double, 3, 2)(&md);
    mt_mat_set_row(double, 3, 2)(&md, 2, row);
    if (mt_mat_at(double, 3, 2)(&md, 2, 0) != 1.5) return 30;
    if (mt_mat_at(double, 3, 2)(&md, 2, 1) != 2.5) return 31;
    double got[2];
    mt_mat_get_row(double, 3, 2)(&md, 2, got);
    if (got[0] != 1.5 || got[1] != 2.5) return 32;
    mt_mat_set_col(double, 3, 2)(&md, 0, col);
    if (mt_mat_at(double, 3, 2)(&md, 0, 0) != 10.0) return 33;
    if (mt_mat_at(double, 3, 2)(&md, 1, 0) != 20.0) return 34;
    if (mt_mat_at(double, 3, 2)(&md, 2, 0) != 30.0) return 35;

    /* ---- 5. 转置: 3×2 → 2×3 ---- */
    mt_mat(int, 3, 2) A;
    mt_mat(int, 2, 3) At;
    mt_mat_fill(int, 3, 2)(&A, 0);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 2; j++)
            mt_mat_set(int, 3, 2)(&A, i, j, i * 10 + j);
    mt_mat_transpose(int, 3, 2)(&At, &A);
    /* A[i][j] → At[j][i] */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 2; j++)
            if (mt_mat_at(int, 2, 3)(&At, j, i) != i * 10 + j) return 40;
    if (mt_mat_at(int, 2, 3)(&At, 1, 2) != 21) return 41;
    if (mt_mat_at(int, 2, 3)(&At, 0, 2) != 20) return 42;

    /* ---- 6. 尺寸 getter(编译期常量) ---- */
    if (mt_mat_rows(double, 3, 2)() != 3) return 50;
    if (mt_mat_cols(double, 3, 2)() != 2) return 51;
    if (mt_mat_size(double, 3, 2)() != 6) return 52;
    if (mt_mat_rows(float, 1, 8)() != 1) return 53;

    /* ---- 大尺寸实例: 编译期尺寸驱动(行主序尾部元素) ---- */
    mt_mat(float, 4, 4) m4;
    mt_mat_fill(float, 4, 4)(&m4, 3.0f);
    if (mt_mat_at(float, 4, 4)(&m4, 3, 3) != 3.0f) return 60;
    if (sizeof(m4.a) != 16 * sizeof(float)) return 61;

    return 0;
}