/* tcc-stl iterator.h - 迭代器模型 (双轨)
 *
 * 决策(§13-3): 连续容器(vector/string)用裸指针(零开销); 链式/list 用抽象迭代器。
 *
 * M0a 交付重点是"连续容器裸指针"约定:
 *   model (T) T *Vector_data(Vector(T) *self);        // begin
 *   model (T) T *Vector_end  (Vector(T) *self);       // = data + len
 * 遍历: for (T *it = Vector_data(int)(&v); it != Vector_end(int)(&v); ++it) ...
 * 逆序/范围算法(M0c algorithm.h)均接受 (begin,end) 指针区间。
 *
 * 抽象迭代器接口表(vptr/itab, 项目惯例)供 M0b List 用; M0a 仅声明骨架。
 * 约定: 接口表每函数首参必须 void *self(itab 自动生成前提); 对象内嵌 ops 指针。
 */
#ifndef SLT_ITERATOR_H
#define SLT_ITERATOR_H

/* 链式容器抽象迭代器的方法表(vptr 指向它)。M0b 给 ListIter(T) 用。
 * 现只声明宽度, 具体类型方法在 list.h 填充。 */
typedef struct slt_iter_ops {
    void  (*incr)(void *self);        /* 前进一个元素 */
    void *(*deref)(void *self);       /* 取当前元素地址(可为 0 表示 end) */
    int   (*eq)  (const void *a, const void *b);  /* 两迭代器判等 */
} slt_iter_ops;

#endif /* SLT_ITERATOR_H */