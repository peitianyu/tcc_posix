/* tcc-stl algorithm.h - 基础算法 (model function 泛型)
 *
 * 区间算法对连续容器传裸指针区间; 元素比较/判等默认走 T 的原生运算符:
 *   - 内置标量(int/double/指针…): 原生 `<` / `==`;
 *   - 用户自定义值类型: 为其具体类型声明 `operator<`(→ operator_lt) / `operator==`
 *     (→ operator_eq), model 泛型重放时 `a < b`/`a == b` 自动分发(编译期静态分派)。
 *
 * 命名仿 STL 加 `stl_` 前缀避免冲突。方法 `model (T)` 泛型函数、显式实例化调用
 * `stl_sort(int)(arr, n)`; 与容器方法同款(无对象方法糖)。
 */
#ifndef STL_ALGORITHM_H
#define STL_ALGORITHM_H

/* 依赖 allocator.h(STL_ALIGN 等; 保持与其余 STL 头一致的 self-contained 风格)。 */
#include "allocator.h"

/* 查找: 返回首个 *b == val 的指针, 无则 e */
model (T) T *stl_find(T *b, T *e, T val) {
    for (; b != e; b++) if (*b == val) return b;
    return e;
}

/* 计数: 区间内 == val 的元素个数 */
model (T) int stl_count(T *b, T *e, T val) {
    int c = 0;
    for (; b != e; b++) if (*b == val) c++;
    return c;
}

/* 填值: [b,e) 全部赋 val */
model (T) void stl_fill(T *b, T *e, T val) {
    for (; b != e; b++) *b = val;
}

/* 反转: [b,e) 就地逆序 */
model (T) void stl_reverse(T *b, T *e) {
    for (e--; b < e; b++, e--) { T t = *b; *b = *e; *e = t; }
}

/* 遍历: 对 [b,e) 逐元素调 fn(T*) */
model (T) void stl_for_each(T *b, T *e, void (*fn)(T *)) {
    for (; b != e; b++) fn(b);
}

/* 选择排序(正确性优先): a[0..n-1] 排序, 默认 a[j] < a[m] */
model (T) void stl_sort(T *a, int n) {
    for (int i = 0; i < n - 1; i++) {
        int m = i;
        for (int j = i + 1; j < n; j++)
            if (a[j] < a[m]) m = j;
        if (m != i) { T t = a[i]; a[i] = a[m]; a[m] = t; }
    }
}

/* 极值: 输出最小/最大到 *lo/*hi(可为 0), 用 < 与 > 双端比较 */
model (T) void stl_minmax(T *a, int n, T *lo, T *hi) {
    if (n <= 0) return;
    T mn = a[0], mx = a[0];
    for (int i = 1; i < n; i++) {
        if (a[i] < mn) mn = a[i];
        if (mx < a[i]) mx = a[i];
    }
    if (lo) *lo = mn;
    if (hi) *hi = mx;
}

#endif /* STL_ALGORITHM_H */