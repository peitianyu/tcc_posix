/* tcc-stl list.h - model List(T)（双向链表, self-contained）
 *
 * 约束（docs/stl.md §13）：
 *   - 方法 `model (T)` 泛型函数、显式实例化调用 `List_push_back(int)(&l, x)`（无对象方法糖）。
 *   - 节点用**方法体内的宏展开局部结构** `struct __sl_node_ { prev;next;T data; }`，
 *     规避 model struct 递归自引用；同样绕开"泛型函数内调另一泛型 unresolved"。
 *   - 分配走 self-contained `slt_arena_alloc`（musl malloc；仅 musl 标准头）。
 *   - 迭代 = 节点指针（裸 `void*`）：List_begin/next/data/end，零依赖。
 *
 * 空链表 head==0；List_front/back 在 SLT_CHECKS 下断言非空。
 */
#ifndef SLT_LIST_H
#define SLT_LIST_H

#include "allocator.h"

/* 在泛型方法体内展开局部节点布局(T 在泛型重放时替换) */
#define SLT_NODE_DEF(T) \
    struct __sl_node_ { struct __sl_node_ *prev, *next; T data; }

model struct List(T) {
    void *head;        /* 首节点(struct __sl_node_*) */
    void *tail;        /* 尾节点 */
    int  len;
    SLT_Arena *ar;
};

model (T) void List_init(List(T) *self, SLT_Arena *ar) {
    self->head = self->tail = 0; self->len = 0; self->ar = ar;
}
model (T) int List_size(const List(T) *self) { return self->len; }
model (T) int List_empty(const List(T) *self) { return self->len == 0; }

/* 迭代（节点指针） */
model (T) void *List_begin(const List(T) *self) { return self->head; }
model (T) void *List_end(const List(T) *self)   { return 0; }
model (T) void *List_next(const List(T) *self, void *n) {
    (void)self; SLT_NODE_DEF(T); return n ? ((struct __sl_node_ *)n)->next : 0;
}
model (T) T *List_data(const List(T) *self, void *n) {
    (void)self; SLT_NODE_DEF(T); return n ? &((struct __sl_node_ *)n)->data : 0;
}

model (T) T List_front(const List(T) *self) {
    SLT_NODE_DEF(T); SLT_ASSERT(self->head != 0);
    return ((struct __sl_node_ *)self->head)->data;
}
model (T) T List_back(const List(T) *self) {
    SLT_NODE_DEF(T); SLT_ASSERT(self->tail != 0);
    return ((struct __sl_node_ *)self->tail)->data;
}

model (T) void List_push_back(List(T) *self, T x) {
    SLT_NODE_DEF(T);
    struct __sl_node_ *nn = (struct __sl_node_ *)
        slt_arena_alloc(self->ar, sizeof(struct __sl_node_), SLT_ALIGN);
    if (!nn) return;
    nn->data = x; nn->next = 0; nn->prev = (struct __sl_node_ *)self->tail;
    if (self->tail) ((struct __sl_node_ *)self->tail)->next = nn;
    else            self->head = nn;
    self->tail = nn; self->len++;
}

model (T) void List_push_front(List(T) *self, T x) {
    SLT_NODE_DEF(T);
    struct __sl_node_ *nn = (struct __sl_node_ *)
        slt_arena_alloc(self->ar, sizeof(struct __sl_node_), SLT_ALIGN);
    if (!nn) return;
    nn->data = x; nn->prev = 0; nn->next = (struct __sl_node_ *)self->head;
    if (self->head) ((struct __sl_node_ *)self->head)->prev = nn;
    else            self->tail = nn;
    self->head = nn; self->len++;
}

model (T) void List_pop_back(List(T) *self) {
    SLT_NODE_DEF(T);
    struct __sl_node_ *t;
    if (!self->tail) return;
    t = (struct __sl_node_ *)self->tail;
    self->tail = t->prev;
    if (self->tail) ((struct __sl_node_ *)self->tail)->next = 0;
    else            self->head = 0;
    self->len--;
}

model (T) void List_pop_front(List(T) *self) {
    SLT_NODE_DEF(T);
    struct __sl_node_ *h;
    if (!self->head) return;
    h = (struct __sl_node_ *)self->head;
    self->head = h->next;
    if (self->head) ((struct __sl_node_ *)self->head)->prev = 0;
    else            self->tail = 0;
    self->len--;
}

model (T) void List_clear(List(T) *self) {
    self->head = self->tail = 0; self->len = 0;   /* 节点留 arena, 整池回收 */
}

#endif /* SLT_LIST_H */