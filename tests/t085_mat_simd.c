/* t085_mat_simd: model packet 层 + SIMD 向量算子 (M2)
 *
 * 纯断言(无 stdio)。覆盖 lib/mat/packet.h + pack_sse.h M2 语义, 验证向量化结果与
 * 标量参考逐位等价:
 *   1. packet 尺寸: lane(float)=4, lane(double)=2, 无后端类型(int)=0
 *   2. can_vec: n 为 lane 整数倍 & 16B 对齐才允许向量化; 否则 0
 *   3. 向量算子 add/sub/mul/scal/sqrt: 与标量循环结果一致(含 n 非 lane 整数倍、
 *      未对齐基址的标量回退路径)
 *   4. 便捷算子 add/sub/mul/scal 经 packet 路径(对齐且 n%lane==0 时)结果正确
 *   5. int 等无 SIMD 类型走标量兜底, 结果仍正确
 *
 * 调用风格: 显式实例化 `mt_mat_vec_*(T)(...)` / `mt_packet_lanes(T)()`。
 * 退出码 0 = 通过.
 */
#include "lib/mat/ops.h"
#include <math.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* 与标量参考逐元素比较(f 与参考一致; d 近似的通用容差), 容差小值避开非确定 */
static int near_f(float x, float y) { return fabsf(x - y) < 1e-5f; }
static int near_d(double x, double y) { return fabs(x - y) < 1e-9; }

/* 参考标量实现: 待测语义的"朴素版"(不继承自被测的后端) */
static float ref_add(float a, float b) { return a + b; }
static float ref_sub(float a, float b) { return a - b; }
static float ref_mul(float a, float b) { return a * b; }

int main(void) {
    /* ---- 1. packet 尺寸 ---- */
    CHECK(mt_packet_lanes(float)()  == 4);
    CHECK(mt_packet_lanes(double)() == 2);
    CHECK(mt_packet_lanes(int)()    == 0);

    /* ---- 2. can_vec 判定 ---- */
    _Alignas(16) float af[32];
    CHECK(mt_packet_can_vec(float)(af, 8) == 1);   /* n%4==0 且 16B 对齐 */
    CHECK(mt_packet_can_vec(float)(af, 6) == 0);   /* n%4!=0 */
    CHECK(mt_packet_can_vec(float)(af, 0) == 0);   /* 0 元素 */
    CHECK(mt_packet_can_vec(float)(af + 1, 8) == 0); /* 未对齐 */
    CHECK(mt_packet_can_vec(int)(af, 32) == 0);    /* 类型无后端 */
    _Alignas(16) double ad[16];
    CHECK(mt_packet_can_vec(double)(ad, 8) == 1);  /* n%2==0 且对齐 */

    /* ---- 3. 向量算子与标量参考一致 (含尾标量 / 未对齐回退) ----
     * 对齐基址 off=0 时向量走 load+op+store; off=1 时整段标量兜底(不越界用 64 元素缓冲)。 */
    {
        _Alignas(16) float fa[64], fb[64], fr[64];
        _Alignas(16) double da[64], db[64], dr[64];
        int n = 32;
        for (int off = 0; off < 2; off++) {
            float *a = fa + off, *b = fb + off;
            for (int i = 0; i < n; i++) { a[i] = (float)(i + 1); b[i] = (float)(i * 2); }
            mt_mat_vec_add(float)(fr + off, a, b, n);
            for (int i = 0; i < n; i++) CHECK(near_f(fr[off + i], ref_add(a[i], b[i])));
            mt_mat_vec_sub(float)(fr + off, a, b, n);
            for (int i = 0; i < n; i++) CHECK(near_f(fr[off + i], ref_sub(a[i], b[i])));
            mt_mat_vec_mul(float)(fr + off, a, b, n);
            for (int i = 0; i < n; i++) CHECK(near_f(fr[off + i], ref_mul(a[i], b[i])));
            mt_mat_vec_scal(float)(fr + off, a, 2.5f, n);
            for (int i = 0; i < n; i++) CHECK(near_f(fr[off + i], a[i] * 2.5f));
            mt_mat_vec_sqrt(float)(fr + off, a, n);
            for (int i = 0; i < n; i++) CHECK(near_f(fr[off + i], sqrtf(a[i])));
            /* 非 lane 整数倍(n=6): 对齐时走 4 块 + 2 尾标量; 未对齐时整段标量 */
            mt_mat_vec_add(float)(fr + off, a, b, 6);
            for (int i = 0; i < 6; i++) CHECK(near_f(fr[off + i], ref_add(a[i], b[i])));
        }
        /* double: 2 lane (n=10 触发尾标量) */
        for (int i = 0; i < 16; i++) { da[i] = (double)(i + 1) * 0.5; db[i] = (double)(i - 3); }
        mt_mat_vec_add(double)(dr, da, db, 10);
        for (int i = 0; i < 10; i++) CHECK(near_d(dr[i], da[i] + db[i]));
        mt_mat_vec_sub(double)(dr, da, db, 10);
        for (int i = 0; i < 10; i++) CHECK(near_d(dr[i], da[i] - db[i]));
        mt_mat_vec_mul(double)(dr, da, db, 16);
        for (int i = 0; i < 16; i++) CHECK(near_d(dr[i], da[i] * db[i]));
        mt_mat_vec_scal(double)(dr, da, 3.0, 16);
        for (int i = 0; i < 16; i++) CHECK(near_d(dr[i], da[i] * 3.0));
        mt_mat_vec_sqrt(double)(dr, da, 16);
        for (int i = 0; i < 16; i++) CHECK(near_d(dr[i], sqrt(da[i])));
    }

    /* ---- 4. 便捷算子走 packet 路径: 对齐且 n%lane==0 应命中向量化(结果仍正确) ---- */
    {
        mt_mat(float, 2, 4) A, B, C;              /* n=8, float → can_vec==1 */
        mt_mat_fill(float, 2, 4)(&A, 3.0f);
        mt_mat_fill(float, 2, 4)(&B, 2.0f);
        mt_mat_add(float, 2, 4)(&C, &A, &B);
        for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], 5.0f));
        mt_mat_sub(float, 2, 4)(&C, &A, &B);
        for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], 1.0f));
        mt_mat_mul(float, 2, 4)(&C, &A, &B);
        for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], 6.0f));
        mt_mat_scal(float, 2, 4)(&C, &A, 4.0f);
        for (int i = 0; i < 8; i++) CHECK(near_f(C.a[i], 12.0f));
    }

    /* ---- 5. 无 SIMD 类型(int): lane=0 → 便捷算子走标量兜底, 结果正确 ---- */
    {
        mt_mat(int, 2, 4) A, B, C;
        mt_mat_fill(int, 2, 4)(&A, 10);
        mt_mat_fill(int, 2, 4)(&B, 4);
        mt_mat_add(int, 2, 4)(&C, &A, &B);
        for (int i = 0; i < 8; i++) CHECK(C.a[i] == 14);
        mt_mat_mul(int, 2, 4)(&C, &A, &B);
        for (int i = 0; i < 8; i++) CHECK(C.a[i] == 40);
        mt_mat_scal(int, 2, 4)(&C, &A, 3);
        for (int i = 0; i < 8; i++) CHECK(C.a[i] == 30);
    }

    return 0;
}