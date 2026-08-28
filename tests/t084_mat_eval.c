/* t084_mat_eval: model mt_expr(T) 融合求值通道 (M1)
 *
 * 纯断言(无 stdio)。覆盖(对齐 lib/mat/ops.h M1 语义):
 *   1. 叶子 SRC / BCAST: 单源引用与标量广播
 *   2. 便捷算子: add/sub/mul(逐元素)、scal(标量乘)、un(一元)
 *   3. 复合表达式: (A + B) * 2、A - B、scal(A) 嵌套
 *   4. 不悬挂验证: 节点在同一作用域构造并消费, dst 独立于叶子
 *   5. 每元素正确性: 行主序线性下标在大/非方形实例上对齐
 *
 * 调用风格: 显式实例化 `mt_mat_*(T,R,C)(...)` / `mt_expr_*(T)(...)`。
 * 退出码 0 = 通过.
 */
#include "lib/mat/ops.h"
#include <stdint.h>
#include <math.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* 用便捷算子, dst 独立矩阵, 验证逐元素结果 */
static int test_basic(void) {
    mt_mat(float, 2, 3) A, B, C;
    mt_mat_fill(float, 2, 3)(&A, 2.0f);
    mt_mat_fill(float, 2, 3)(&B, 3.0f);

    mt_mat_add(float, 2, 3)(&C, &A, &B);          /* C = 2+3 = 5 */
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++)
            if (mt_mat_at(float, 2, 3)(&C, i, j) != 5.0f) return 1;

    mt_mat_sub(float, 2, 3)(&C, &A, &B);          /* C = 2-3 = -1 */
    if (mt_mat_at(float, 2, 3)(&C, 0, 0) != -1.0f) return 2;
    if (mt_mat_at(float, 2, 3)(&C, 1, 2) != -1.0f) return 3;

    mt_mat_mul(float, 2, 3)(&C, &A, &B);          /* C = 2*3 = 6 */
    if (mt_mat_at(float, 2, 3)(&C, 1, 1) != 6.0f) return 4;

    mt_mat_scal(float, 2, 3)(&C, &A, 4.0f);        /* C = A*4 = 8 */
    for (int i = 0; i < 6; i++) if (C.a[i] != 8.0f) return 5;

    mt_mat_un(float, 2, 3)(&C, &A, sqrtf);         /* C = sqrt(2) */
    for (int i = 0; i < 6; i++)
        if (fabsf(C.a[i] - sqrtf(2.0f)) > 1e-6f) return 6;

    return 0;
}

/* 用裸 desc + eval 构造复合表达式: dst = scal( add(A,B), 2 ) = (A+B)*2 */
static int test_composite(void) {
    mt_mat(float, 3, 3) A, B, D;
    mt_mat_fill(float, 3, 3)(&A, 1.0f);
    mt_mat_fill(float, 3, 3)(&B, 2.0f);

    mt_expr(float) la = mt_expr_src(float)(A.a);
    mt_expr(float) rb = mt_expr_src(float)(B.a);
    mt_expr(float) sum = mt_expr_bin(float)(MT_OP_ADD, &la, &rb);
    mt_expr(float) root = mt_expr_scal(float)(&sum, 2.0f);   /* (A+B)*2 = 6 */

    if (mt_mat_eval(float, 3, 3)(&D, &root) != 0) return 1;
    for (int i = 0; i < 9; i++) if (D.a[i] != 6.0f) return 2;

    /* 裸露 mt_expr_get: 逐元素取 SRC/BCAST */
    mt_expr(float) src = mt_expr_src(float)(A.a);
    mt_expr(float) bsc = mt_expr_bcast(float)(42.0f);
    if (mt_expr_get(float)(&src, 4) != 1.0f) return 3;   /* A.a[4]=1 */
    if (mt_expr_get(float)(&bsc, 0) != 42.0f) return 4;

    return 0;
}

/* 不悬挂 + 尺寸独立性: dst 与叶子不同对象, 非方形大实例线性下标对齐 */
static int test_lifetime(void) {
    mt_mat(double, 3, 4) A, B, C;
    mt_mat_fill(double, 3, 4)(&A, 1.0);
    mt_mat_fill(double, 3, 4)(&B, 10.0);
    mt_mat_add(double, 3, 4)(&C, &A, &B);          /* C = 11 */
    for (int i = 0; i < 12; i++)
        if (mt_mat_at(double, 3, 4)(&C, i / 4, i % 4) != 11.0) return 1;

    /* 叶子 A/B 未被覆盖, 值仍保留 (dst 独立) */
    if (mt_mat_at(double, 3, 4)(&A, 0, 0) != 1.0) return 2;
    if (mt_mat_at(double, 3, 4)(&B, 2, 3) != 10.0) return 3;

    /* 混合标量 + 逐元素: dst = A*2 + B → 12 */
    mt_expr(double) la = mt_expr_src(double)(A.a);
    mt_expr(double) rb = mt_expr_src(double)(B.a);
    mt_expr(double) sA = mt_expr_scal(double)(&la, 2.0);
    mt_expr(double) root = mt_expr_bin(double)(MT_OP_ADD, &sA, &rb);
    mt_mat_eval(double, 3, 4)(&C, &root);
    for (int i = 0; i < 12; i++)
        if (mt_mat_at(double, 3, 4)(&C, i / 4, i % 4) != 12.0) return 4;

    return 0;
}

int main(void) {
    int r;
    if ((r = test_basic()) != 0) return r;
    if ((r = test_composite()) != 0) return 100 + r;
    if ((r = test_lifetime()) != 0) return 200 + r;
    return 0;
}