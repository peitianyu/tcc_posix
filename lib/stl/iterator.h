/* tcc-stl iterator.h - 迭代器模型 (双轨: 连续裸指针 / 链式抽象迭代器)
 *
 * 决策(§6): 连续容器(vector/deque/string)用裸指针(零开销); 链式容器(list)用抽象
 * 迭代器——对象内嵌 vptr(方法表), 经 `incr/deref/eq` 间接调用遍历, 与具体布局解耦。
 *
 * 这里交付"M1-待办-抽象迭代器算法骨架": 抽象迭代器对象 + 基于它的算法框架。
 * 算法不再假设元素连续, 只在 `ops` 上做 `incr/deref/eq` 三操作即可遍历任意结构,
 * 使 find/count/for_each 一族能统一作用于链式容器(与 algorithm.h 的裸指针区间并存)。
 *
 * 契约:
 *   - 接口表 `stl_iter_ops` 成员首参须 `void *self`(项目 itab 惯例), 实际入参是
 *     `STL_Iter(T)*`(强转)。`deref` 返回当前元素地址, 越界/end 返回 0。
 *   - 某 T 的这三操作+表由容器端每 T 实例化一次(宏展开 per-T 静态函数)后,
 *     用 `STL_ITER_SET(it, ops, ctx)` 填对象; 泛型算法 body 只经 ops 间接调用,
 *     **不**依赖具体 T 布局(自包含, 无泛型互调)。
 *   - 连续容器照样可直接传指针区间给 algorithm.h, 不必折损为抽象迭代器。
 */
#ifndef STL_ITERATOR_H
#define STL_ITERATOR_H

#include "allocator.h"

/* 抽象迭代器方法表(vptr 指向它)。incr 前进; deref 当前元素地址(0=end);
 * eq 判两迭代器"同一位"(比较 ctx)。 */
typedef struct stl_iter_ops {
    void  (*incr)(void *self);
    void *(*deref)(void *self);
    int   (*eq)  (const void *a, const void *b);
} stl_iter_ops;

/* 抽象迭代器对象: 内嵌 vptr + 无类型遍历存量(ctx, 如链表节点指针) */
model struct STL_Iter(T) {
    const stl_iter_ops *ops;   /* vptr */
    void *ctx;                 /* 容器具体遍历状态 */
};

/* 填对象(容器端每 T 实例化一次 ops 后将 ctx 置初态) */
#define STL_ITER_SET(it, ps, c) do { (it).ops = (ps); (it).ctx = (void *)(c); } while (0)

/* ---- 基于抽象迭代器的泛型算法骨架 (仅经 vptr, 全 T 一份) ---- */

/* 查找: 返回首个 `*p == val` 的元素地址; 区间无则 0(不覆盖裸指针 stl_find) */
model (T) T *stl_iter_find(STL_Iter(T) it, STL_Iter(T) end, T val) {
    while (1) {
        if (it.ops->eq(&it, &end)) return 0;
        void *p = it.ops->deref(&it);
        if (p && *(T *)p == val) return (T *)p;
        it.ops->incr(&it);
    }
}

/* 计数: 区间内 == val 的元素个数 */
model (T) int stl_iter_count(STL_Iter(T) it, STL_Iter(T) end, T val) {
    int c = 0;
    while (1) {
        if (it.ops->eq(&it, &end)) return c;
        void *p = it.ops->deref(&it);
        if (p && *(T *)p == val) c++;
        it.ops->incr(&it);
    }
}

/* 遍历: 对区间内每元素调 fn(T*) */
model (T) void stl_iter_for_each(STL_Iter(T) it, STL_Iter(T) end, void (*fn)(T *)) {
    while (1) {
        if (it.ops->eq(&it, &end)) return;
        void *p = it.ops->deref(&it);
        if (p) fn((T *)p);
        it.ops->incr(&it);
    }
}

#endif /* STL_ITERATOR_H */