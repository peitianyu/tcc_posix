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

/* ---- 算法中断(经抽象迭代器语义落到 list): 内联遍历, 避免 per-T 静态表重复
 * 定义(list 已有裸节点 span, 这里提供 find/count/for_each 三件套; 通用 vptr 抽象
 * 迭代器算法骨架见 iterator.h §"M1-待办-抽象迭代器算法骨架") ---- */

model (T) T *stl_list_find(const STL_List(T) *self, T val) {
    STL_NODE_DEF(T);
    struct __stl_node_ *n = (struct __stl_node_ *)self->head;
    for (; n; n = n->next) if (n->data == val) return &n->data;
    return 0;
}
model (T) int stl_list_count(const STL_List(T) *self, T val) {
    STL_NODE_DEF(T);
    struct __stl_node_ *n = (struct __stl_node_ *)self->head;
    int c = 0;
    for (; n; n = n->next) if (n->data == val) c++;
    return c;
}
model (T) void stl_list_for_each(const STL_List(T) *self, void (*fn)(T *)) {
    STL_NODE_DEF(T);
    struct __stl_node_ *n = (struct __stl_node_ *)self->head;
    for (; n; n = n->next) fn(&n->data);
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

/* 在节点 after 后插入 x(after 可为迭代出的任一节点; after==0 → 头插, 等价 push_front)。
 * 自包含内联(§13-12), 不调其它泛型方法。分配失败保持原位。 */
model (T) void stl_list_insert(STL_List(T) *self, void *after, T x) {
    STL_NODE_DEF(T);
    struct __stl_node_ *a = (struct __stl_node_ *)after;
    struct __stl_node_ *nn = (struct __stl_node_ *)
        stl_arena_alloc(self->ar, sizeof(struct __stl_node_), STL_ALIGN);
    if (!nn) return;
    nn->data = x;
    if (!a) {                             /* 头插 */
        nn->prev = 0; nn->next = (struct __stl_node_ *)self->head;
        if (self->head) ((struct __stl_node_ *)self->head)->prev = nn;
        else            self->tail = nn;
        self->head = nn;
    } else {                              /* 插到 a 之后 */
        nn->prev = a; nn->next = a->next;
        if (a->next) ((struct __stl_node_ *)a->next)->prev = nn;
        else         self->tail = nn;
        a->next = nn;
    }
    self->len++;
}

/* 删除给定节点 n(须属 self; 由迭代/遍历得到). 无则 no-op. */
model (T) void stl_list_erase(STL_List(T) *self, void *n) {
    STL_NODE_DEF(T);
    struct __stl_node_ *d = (struct __stl_node_ *)n;
    if (!d) return;
    if (d->prev) d->prev->next = d->next; else self->head = d->next;
    if (d->next) d->next->prev = d->prev; else self->tail = d->prev;
    self->len--;
}

/* 深拷贝: 逐节点 data 重建新链表(节点自 self->ar 分配), 按值返回, 不改 self. */
model (T) STL_List(T) stl_list_copy(const STL_List(T) *self) {
    STL_NODE_DEF(T);
    STL_List(T) r;
    r.head = r.tail = 0; r.len = 0; r.ar = self->ar;
    struct __stl_node_ *n = (struct __stl_node_ *)self->head;
    while (n) {
        struct __stl_node_ *nn = (struct __stl_node_ *)
            stl_arena_alloc(self->ar, sizeof(struct __stl_node_), STL_ALIGN);
        if (!nn) break;
        nn->data = n->data; nn->next = 0; nn->prev = (struct __stl_node_ *)r.tail;
        if (r.tail) ((struct __stl_node_ *)r.tail)->next = nn;
        else        r.head = nn;
        r.tail = nn; r.len++;
        n = n->next;
    }
    return r;
}

#endif /* STL_LIST_H */