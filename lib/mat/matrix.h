/* matrix.h - 固定尺寸稠密矩阵存储层 (M0)
 *
 * `mt_mat(T,R,C)`: 行主序连续存储, 栈上 POD, 零堆, 16B 对齐(SIMD 前提)。
 * 对标 Eigen `Matrix<T,R,C>` 的固定尺寸形态; R/C 为 model 常量参, 尺寸编译期确定
 * (见 docs/matrix.md §5)。
 *
 * 本文件只含**存储/访问**语义(§5), 不做运算/求值(§6+ 属 ops 层)。
 * 元素为 T[R*C] 字段 + 强制 16B 对齐; 数据区为 struct 首成员(offset 0), `_Alignas(16)`
 * 保证类型级对齐。运行时若某缓冲基址未 16B 对齐, SIMD 层(pack_sse.h)用 `mt_sse_ok16`
 * 运行时守卫整段**标量回退**, 而非 loadu(见 pack_sse.h: TCC 未注册 loadu 名)。
 *
 * 调用风格: 显式实例化 `mt_mat_*(T, R, C)(...)`。
 */
#ifndef MT_MAT_MATRIX_H
#define MT_MAT_MATRIX_H

#include <stddef.h>
#include <assert.h>

/* mat 模块**完全自包含**: 不包含 lib/stl, 无任何外模块依赖。本地断言宏 MT_ASSERT:
 * NDEBUG 时裁剪为 noop(发布), 否则走 <assert.h>。 */
#ifndef MT_ASSERT
# ifdef NDEBUG
#  define MT_ASSERT(c) ((void)0)
# else
#  define MT_ASSERT(c) assert(c)
# endif
#endif

/* 固定尺寸稠密矩阵: T 元素类型, R 行, C 列(model 常量参, 布局期求值)。
 * union: 数组为数据区(offset 0), _Alignas(16) char 强制整个 union 16B 对齐。
 * 注意: 对齐会把结构体 sizeof 圆整到 16B 边界(尺寸未满时补 padding)。 */
model struct mt_mat(T, int R, int C) {
    union {
        T a[R * C];
        _Alignas(16) char _align16;
    };
};

/* --- 尺寸(编译期常量; 经实例化 + _Generic 展开) --- */

model (T, int R, int C) int mt_mat_rows(void) { return R; }
model (T, int R, int C) int mt_mat_cols(void) { return C; }
model (T, int R, int C) int mt_mat_size(void)  { return R * C; }

/* --- 数据指针 --- */

model (T, int R, int C) T *mt_mat_ptr(mt_mat(T,R,C) *m) { return m->a; }
model (T, int R, int C) const T *mt_mat_cptr(const mt_mat(T,R,C) *m) { return m->a; }

/* --- 构造/填充 --- */

/* fill: 全体元素置 v */
model (T, int R, int C) void mt_mat_fill(mt_mat(T,R,C) *m, T v) {
    int i;
    for (i = 0; i < R * C; i++) m->a[i] = v;
}
model (T, int R, int C) void mt_mat_zero(mt_mat(T,R,C) *m) {
    int i;
    for (i = 0; i < R * C; i++) m->a[i] = 0;
}
/* identity: 仅方形(R==C)合法, 主对角置 1; 非方形用法由调用方保证。 */
model (T, int R, int C) void mt_mat_identity(mt_mat(T,R,C) *m) {
    int i, j;
    for (i = 0; i < R; i++)
        for (j = 0; j < C; j++)
            m->a[i * C + j] = (i == j) ? 1 : 0;
}

/* --- 访问(行主序; 越界经 MT_ASSERT 检测) --- */

model (T, int R, int C) T mt_mat_at(const mt_mat(T,R,C) *m, int i, int j) {
    MT_ASSERT(m && i >= 0 && i < R && j >= 0 && j < C);
    return m->a[i * C + j];
}
model (T, int R, int C) void mt_mat_set(mt_mat(T,R,C) *m, int i, int j, T v) {
    MT_ASSERT(m && i >= 0 && i < R && j >= 0 && j < C);
    m->a[i * C + j] = v;
}
/* at_ptr: 元素指针(可用作左值) */
model (T, int R, int C) T *mt_mat_at_ptr(mt_mat(T,R,C) *m, int i, int j) {
    MT_ASSERT(m && i >= 0 && i < R && j >= 0 && j < C);
    return &m->a[i * C + j];
}

/* --- 行/列存取 --- */

model (T, int R, int C) void mt_mat_set_row(mt_mat(T,R,C) *m, int i, const T *src) {
    int j;
    MT_ASSERT(m && src && i >= 0 && i < R);
    for (j = 0; j < C; j++) m->a[i * C + j] = src[j];
}
model (T, int R, int C) void mt_mat_get_row(const mt_mat(T,R,C) *m, int i, T *dst) {
    int j;
    MT_ASSERT(m && dst && i >= 0 && i < R);
    for (j = 0; j < C; j++) dst[j] = m->a[i * C + j];
}
model (T, int R, int C) void mt_mat_set_col(mt_mat(T,R,C) *m, int j, const T *src) {
    int i;
    MT_ASSERT(m && src && j >= 0 && j < C);
    for (i = 0; i < R; i++) m->a[i * C + j] = src[i];
}

/* --- 转置: R×C → C×R(目标形状随源交换) --- */

model (T, int R, int C) void mt_mat_transpose(mt_mat(T,C,R) *dst,
                                               const mt_mat(T,R,C) *src) {
    int i, j;
    MT_ASSERT(dst && src && dst->a != src->a);
    for (i = 0; i < R; i++)
        for (j = 0; j < C; j++)
            dst->a[j * R + i] = src->a[i * C + j];
}

#endif /* MT_MAT_MATRIX_H */