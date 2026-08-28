/* t087_mat_lazy: P1 懒求值指针树 (M4)
 *
 * 纯断言(无 stdio)。覆盖 lib/mat/ops.h 的 M4 语义(见 docs/matrix.md §8):
 *   1. VIEW 维度变换叶子: 懒转置 dst(C×R)=src(R×C)^T, 与 matrix.h mt_mat_transpose
 *      一致; 懒块视图 dst = src 子块(起点 ox,oy)
 *   2. 调用方一次性缓冲 mt_expr_frame: 在帧内按 index 建槽、出根槽再 eval_frame,
 *      复合表达式(标量乘/二元)与 P0 具名局部结果一致
 *   3. 帧内视图槽参与复合: transpose(A) + B —— 视图叶子作为复合子节点
 *   4. 无悬挂/复用: 叶子引用左值 a, dst 独立; frame 复用(clear 后重排新线框)
 *   5. 裸 mt_expr_get 对 VIEW 节点逐元素映射正确
 *
 * 调用风格: 显式实例化 `mt_mat_*(T,R,C)(...)` / `mt_expr_*(T)(...)`。
 * 退出码 0 = 通过.
 */
#include "lib/mat/ops.h"
#include <math.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
#define near_f(x,y) (fabsf((x)-(y)) < 1e-5f)

/* ---- 1. 懒转置: 与手工参考 + matrix.h mt_mat_transpose 一致 ---- */
static int test_lazy_transpose(void) {
    mt_mat(float, 2, 3) A;            /* R=2, C=3 */
    float v[6] = { 1,2,3, 4,5,6 };
    for (int i = 0; i < 6; i++) A.a[i] = v[i];

    /* 裸 expr + eval: dst 是 3×2 */
    mt_mat(float, 3, 2) D, R;
    mt_expr(float) t = mt_expr_transpose(float)(A.a, 2, 3);
    (void) mt_mat_eval(float, 3, 2)(&D, &t);

    /* 手工: D[j][i] = A[i][j] */
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++)
            CHECK(near_f(D.a[j * 2 + i], v[i * 3 + j]));

    /* vs matrix.h 参考实现 (A 为 2×3 → 转置后 3×2, 故 R=2,C=3) */
    mt_mat_transpose(float, 2, 3)(&R, &A);
    for (int k = 0; k < 6; k++) CHECK(D.a[k] == R.a[k]);

    /* 便捷: mt_mat_eval_transpose */
    mt_mat(float, 3, 2) D2;
    mt_mat_eval_transpose(float, 2, 3)(&D2, &A);
    for (int k = 0; k < 6; k++) CHECK(D2.a[k] == R.a[k]);

    /* 裸 get: i 对应输出 j*R+i 映射 */
    mt_expr(float) tv = mt_expr_transpose(float)(A.a, 2, 3);
    CHECK(mt_expr_get(float)(&tv, 0) == 1.0f);   /* D[0][0]=A[0][0] */
    CHECK(mt_expr_get(float)(&tv, 1) == 4.0f);   /* D[0][1]=A[1][0] */
    CHECK(mt_expr_get(float)(&tv, 2) == 2.0f);   /* D[1][0]=A[0][1] */
    CHECK(mt_expr_get(float)(&tv, 5) == 6.0f);   /* D[2][1]=A[1][2] */
    return 0;
}

/* ---- 1b. 懒块视图: dst = src 的子块 ---- */
static int test_lazy_block(void) {
    mt_mat(float, 4, 4) S;            /* 4×4 */
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) S.a[r * 4 + c] = (float)(r * 10 + c);

    /* 起点 (行1,列1) 取 2×2: S[1..2][1..2] (src.a + 源列数 sc=4) */
    mt_mat(float, 2, 2) D;
    mt_mat_eval_block(float, 2, 2)(&D, S.a, 4, 1, 1);
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 2; c++)
            CHECK(near_f(D.a[r * 2 + c], (float)((r + 1) * 10 + (c + 1))));

    /* direct: i=0 → S[1][1]=11 */
    mt_expr(float) blk = mt_expr_block(float)(S.a, 2, 4, 1, 1);
    CHECK(mt_expr_get(float)(&blk, 0) == 11.0f);
    CHECK(mt_expr_get(float)(&blk, 1) == 12.0f);
    CHECK(mt_expr_get(float)(&blk, 2) == 21.0f);
    return 0;
}

/* ---- 2. frame: 复合表达式与 P0 结果一致 ---- */
static int test_frame_composite(void) {
    mt_mat(float, 2, 3) A, B, D;
    for (int i = 0; i < 6; i++) { A.a[i] = (float)(i + 1); B.a[i] = (float)(i + 10); }

    /* P0 参照: dst = 2*(A+B) */
    mt_expr(float) la = mt_expr_src(float)(A.a);
    mt_expr(float) rb = mt_expr_src(float)(B.a);
    mt_expr(float) sum = mt_expr_bin(float)(MT_OP_ADD, &la, &rb);
    mt_expr(float) root = mt_expr_scal(float)(&sum, 2.0f);
    (void) mt_mat_eval(float, 2, 3)(&D, &root);

    /* P1 frame 同语义 */
    mt_expr_frame(float) f;
    mt_expr_frame_clear(float)(&f);
    int sA = mt_expr_slot_src(float)(&f, A.a);
    int sB = mt_expr_slot_src(float)(&f, B.a);
    int sSum = mt_expr_slot_bin(float)(&f, MT_OP_ADD, sA, sB);
    int sRoot = mt_expr_slot_scal(float)(&f, sSum, 2.0f);
    mt_mat(float, 2, 3) DF;
    (void) mt_mat_eval_frame(float, 2, 3)(&DF, &f, sRoot);

    for (int i = 0; i < 6; i++) CHECK(near_f(DF.a[i], D.a[i]));

    /* 更深复合: A*B + scal(C) 双层 (多个槽) */
    mt_mat(float, 2, 2) X, Y, Z, DX;
    float vx[4] = { 1,2, 3,4 }, vy[4] = { 5,6, 7,8 };
    for (int i = 0; i < 4; i++) { X.a[i] = vx[i]; Y.a[i] = vy[i]; }
    /* 参照: mask = X*Y + X*2 */
    mt_expr(float) x0 = mt_expr_src(float)(X.a), y0 = mt_expr_src(float)(Y.a);
    mt_expr(float) xy = mt_expr_bin(float)(MT_OP_MUL, &x0, &y0);
    mt_expr(float) x2 = mt_expr_scal(float)(&x0, 2.0f);
    mt_expr(float) big = mt_expr_bin(float)(MT_OP_ADD, &xy, &x2);
    (void) mt_mat_eval(float, 2, 2)(&Z, &big);

    mt_expr_frame(float) g; mt_expr_frame_clear(float)(&g);
    int gX = mt_expr_slot_src(float)(&g, X.a);
    int gY = mt_expr_slot_src(float)(&g, Y.a);
    int gXY = mt_expr_slot_bin(float)(&g, MT_OP_MUL, gX, gY);
    int gX2 = mt_expr_slot_scal(float)(&g, gX, 2.0f);
    int gBig = mt_expr_slot_bin(float)(&g, MT_OP_ADD, gXY, gX2);
    (void) mt_mat_eval_frame(float, 2, 2)(&DX, &g, gBig);
    for (int i = 0; i < 4; i++) CHECK(near_f(DX.a[i], Z.a[i]));

    return 0;
}

/* ---- 3. 帧内视图槽参与复合: dst(3×2) = transpose(A(2×3)) + B(3×2) ---- */
static int test_frame_view_composite(void) {
    mt_mat(float, 2, 3) A;
    mt_mat(float, 3, 2) B, D, R;
    float va[6] = { 1,2,3, 4,5,6 };
    float vb[6] = { 10,20, 30,40, 50,60 };
    for (int i = 0; i < 6; i++) { A.a[i] = va[i]; B.a[i] = vb[i]; }

    /* 参照: 先转置 dst, 再 dst + B */
    mt_mat_eval_transpose(float, 2, 3)(&R, &A);      /* R 3×2 = A^T */
    for (int i = 0; i < 6; i++) R.a[i] += vb[i];      /* R = A^T + B */

    /* P1: 帧内 转置槽 + src(B) 槽 + ADD */
    mt_expr_frame(float) f; mt_expr_frame_clear(float)(&f);
    int sT  = mt_expr_slot_view(float)(&f, A.a, /*cols=srcrows*/2, /*srccols=*/3, 0, 0, 1);
    int sB  = mt_expr_slot_src(float)(&f, B.a);
    int sR  = mt_expr_slot_bin(float)(&f, MT_OP_ADD, sT, sB);
    (void) mt_mat_eval_frame(float, 3, 2)(&D, &f, sR);
    for (int i = 0; i < 6; i++) CHECK(near_f(D.a[i], R.a[i]));

    return 0;
}

/* ---- 4. 无悬挂 + 帧复用: 叶子引用左值, dst 独立; frame clear 后重排 ----- */
static int test_no_dangling(void) {
    mt_mat(float, 2, 2) A, B, C;
    A.a[0]=1; A.a[1]=2; A.a[2]=3; A.a[3]=4;
    for (int i = 0; i < 4; i++) B.a[i] = (float)(i + 1) * 10.0f;

    mt_expr_frame(float) f; mt_expr_frame_clear(float)(&f);
    int sA = mt_expr_slot_src(float)(&f, A.a);
    int sB = mt_expr_slot_src(float)(&f, B.a);
    int sAB = mt_expr_slot_bin(float)(&f, MT_OP_MUL, sA, sB);
    (void) mt_mat_eval_frame(float, 2, 2)(&C, &f, sAB);

    /* dst 正确且叶子未变 */
    for (int i = 0; i < 4; i++) CHECK(near_f(C.a[i], A.a[i] * B.a[i]));
    CHECK(A.a[0] == 1.0f && A.a[3] == 4.0f);
    CHECK(B.a[2] == 30.0f);

    /* 帧复用: 复用同一 frame, 重新建线框(不同树) */
    mt_expr_frame_clear(float)(&f);
    int s2A = mt_expr_slot_src(float)(&f, A.a);
    int s2 = mt_expr_slot_scal(float)(&f, s2A, 5.0f);   /* 5*A */
    (void) mt_mat_eval_frame(float, 2, 2)(&C, &f, s2);
    for (int i = 0; i < 4; i++) CHECK(near_f(C.a[i], A.a[i] * 5.0f));

    return 0;
}

int main(void) {
    int r;
    if ((r = test_lazy_transpose()) != 0) return r;
    if ((r = test_lazy_block()) != 0) return 100 + r;
    if ((r = test_frame_composite()) != 0) return 200 + r;
    if ((r = test_frame_view_composite()) != 0) return 300 + r;
    if ((r = test_no_dangling()) != 0) return 400 + r;
    return 0;
}