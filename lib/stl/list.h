/* tcc-stl list.h - model STL_List(T)（双向链表, self-contained）
 *
 * 约束（docs/stl.md §13）：
 *   - 方法 `model (T)` 泛型函数、显式实例化调用 `stl_list_push_back(int)(&l, x)`（无对象方法糖）。
 *   - 节点用**方法体内的宏展开局部结构** `struct __stl_node_ { prev;next;T data; }`，
 *     规避 model struct 递归自引用；同样绕开"泛型函数内调另一泛型 unresolved"。
 *   - 分配走 self-contained `stl_arena_alloc`（musl malloc；仅 musl 标准头）。
 *   - 迭代 = 节点指针（裸 `void*`）：stl_list_begin/next/data/end，零依赖。
 *
 * 空链表 head==0；stl_list_front/back 在 STL_CHECKS 下断言非空。
 */
#ifndef STL_LIST_H
#define STL_LIST_H

#include "allocator.h"

/* 在泛型方法体内展开局部节点布局(T 在泛型重放时替换) */
#define STL_NODE_DEF(T) \
    struct __stl_node_ { struct __stl_node_ *prev, *next; T data; }

model struct STL_List(T) {
    void *head;        /* 首节点(struct __stl_node_*) */
    void *tail;        /* 尾节点 */
    int  len;
    STL_Arena *ar;
};

model (T) void stl_list_init(STL_List(T) *self, STL_Arena *ar) {
    self->head = self->tail = 0; self->len = 0; self->ar = ar;
}
model (T) int stl_list_size(const STL_List(T) *self) { return self->len; }
model (T) int stl_list_empty(const STL_List(T) *self) { return self->len == 0; }

/* 迭代（节点指针） */
model (T) void *stl_list_begin(const STL_List(T) *self) { return self->head; }
model (T) void *stl_list_end(const STL_List(T) *self)   { return 0; }
model (T) void *stl_list_next(const STL_List(T) *self, void *n) {
    (void)self; STL_NODE_DEF(T); return n ? ((struct __stl_node_ *)n)->next : 0;
}
model (T) T *stl_list_data(const STL_List(T) *self, void *n) {
    (void)self; STL_NODE_DEF(T); return n ? &((struct __stl_node_ *)n)->data : 0;
}

model (T) T stl_list_front(const STL_List(T) *self) {
    STL_NODE_DEF(T); STL_ASSERT(self->head != 0);
    return ((struct __stl_node_ *)self->head)->data;
}
model (T) T stl_list_back(const STL_List(T) *self) {
    STL_NODE_DEF(T); STL_ASSERT(self->tail != 0);
    return ((struct __stl_node_ *)self->tail)->data;
}

model (T) void stl_list_push_back(STL_List(T) *self, T x) {
    STL_NODE_DEF(T);
    struct __stl_node_ *nn = (struct __stl_node_ *)
        stl_arena_alloc(self->ar, sizeof(struct __stl_node_), STL_ALIGN);
    if (!nn) return;
    nn->data = x; nn->next = 0; nn->prev = (struct __stl_node_ *)self->tail;
    if (self->tail) ((struct __stl_node_ *)self->tail)->next = nn;
    else            self->head = nn;
    self->tail = nn; self->len++;
}

model (T) void stl_list_push_front(STL_List(T) *self, T x) {
    STL_NODE_DEF(T);
    struct __stl_node_ *nn = (struct __stl_node_ *)
        stl_arena_alloc(self->ar, sizeof(struct __stl_node_), STL_ALIGN);
    if (!nn) return;
    nn->data = x; nn->prev = 0; nn->next = (struct __stl_node_ *)self->head;
    if (self->head) ((struct __stl_node_ *)self->head)->prev = nn;
    else            self->tail = nn;
    self->head = nn; self->len++;
}

model (T) void stl_list_pop_back(STL_List(T) *self) {
    STL_NODE_DEF(T);
    struct __stl_node_ *t;
    if (!self->tail) return;
    t = (struct __stl_node_ *)self->tail;
    self->tail = t->prev;
    if (self->tail) ((struct __stl_node_ *)self->tail)->next = 0;
    else            self->head = 0;
    self->len--;
}

model (T) void stl_list_pop_front(STL_List(T) *self) {
    STL_NODE_DEF(T);
    struct __stl_node_ *h;
    if (!self->head) return;
    h = (struct __stl_node_ *)self->head;
    self->head = h->next;
    if (self->head) ((struct __stl_node_ *)self->head)->prev = 0;
    else            self->tail = 0;
    self->len--;
}

model (T) void stl_list_clear(STL_List(T) *self) {
    self->head = self->tail = 0; self->len = 0;   /* 节点留 arena, 整池回收 */
}

#endif /* STL_LIST_H */