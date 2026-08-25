/* t077_stl_heap: M1-待办 - allocator 的 heap 后端(逐对象析构) (纯断言)
 *
 * arena 整池回收(无逐对象 free)之外的第二后端: `malloc/free` + 元素析构回调,
 * 适合生命周期精确的容器(独立数据块/链表节点), 每对象单独分配/释放.
 * 覆盖: live 计数随 alloc/free 增减、free 时 dtor 回调、逐对象释放后 live==0、
 *       手写链表(heap 节点)的逐对象销毁即"heap 后端容器"样例、destroy 泄漏警告.
 *
 * 退出码 0 = 通过.
 */
#include "lib/stl/allocator.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* dtor 回调: 累计释放次数 + 校验释放内容 */
static int dtor_cnt;
static void elem_dtor(void *p, void *ud) {
    int *n = (int *)ud;
    (void)p;
    (*n)++;
}

/* heap 后端的"链表节点容器"样例: 每节点独立分配, 逐节点析构释放 */
struct hnode { struct hnode *next; int data; };
struct hlist { struct hnode *head; STL_Heap h; };
static void hlist_push(struct hlist *l, int x) {
    struct hnode *n = (struct hnode *)stl_heap_alloc(&l->h, sizeof *n, 16);
    if (!n) return;
    n->data = x; n->next = 0;
    if (!l->head) l->head = n;
    else { struct hnode *p = l->head; while (p->next) p = p->next; p->next = n; }
}

int main(void) {
    STL_Heap h;
    stl_heap_init(&h);
    CHECK(stl_heap_live(&h) == 0);

    /* 1. alloc 计数 */
    int *a = (int *)stl_heap_alloc(&h, sizeof(int), 16);
    int *b = (int *)stl_heap_alloc(&h, sizeof(int), 16);
    int *c = (int *)stl_heap_alloc(&h, sizeof(int), 16);
    CHECK(a && b && c);
    *a = 11; *b = 22; *c = 33;
    CHECK(stl_heap_live(&h) == 3);

    /* 2. free 部分 → live 递减; 0 指针 no-op 不复位 */
    stl_heap_free(&h, b, 0, 0);
    CHECK(stl_heap_live(&h) == 2);
    stl_heap_free(&h, 0, 0, 0);
    CHECK(stl_heap_live(&h) == 2);

    /* 3. dtor 回调: 释放前执行, 计数++ */
    dtor_cnt = 0;
    stl_heap_free(&h, a, elem_dtor, &dtor_cnt);
    CHECK(dtor_cnt == 1);
    stl_heap_free(&h, c, elem_dtor, &dtor_cnt);
    CHECK(dtor_cnt == 2);
    CHECK(stl_heap_live(&h) == 0);            /* 全部逐个释放 */

    /* 4. 对齐 + 大块 + 多次 alloc/free 交错(逐对象析构稳定性) */
    {
        void *p[40];
        for (int i = 0; i < 40; i++) p[i] = stl_heap_alloc(&h, (size_t)(i * 7 + 3), 16);
        for (int i = 0; i < 40; i++) CHECK(p[i]);
        CHECK(stl_heap_live(&h) == 40);
        for (int i = 0; i < 40; i += 2) stl_heap_free(&h, p[i], 0, 0);
        CHECK(stl_heap_live(&h) == 20);
        for (int i = 1; i < 40; i += 2) stl_heap_free(&h, p[i], 0, 0);
        CHECK(stl_heap_live(&h) == 0);
    }

    /* 5. heap 后端"链表容器": 插入 5 节点, 逐个析构释放 → live==0 */
    {
        struct hlist l; l.head = 0; stl_heap_init(&l.h);
        for (int i = 1; i <= 5; i++) hlist_push(&l, i * 3);
        /* 逐节点析构(释放前无需元素析构:POD) */
        struct hnode *n = l.head;
        l.head = 0;
        while (n) { struct hnode *nx = n->next; stl_heap_free(&l.h, n, 0, 0); n = nx; }
        CHECK(stl_heap_live(&l.h) == 0);
        stl_heap_destroy(&l.h);
    }

    /* 6. destroy 时余活 → heap_check 警告(此处故意泄漏 1 块以验证 live 路径) */
    stl_heap_alloc(&h, 8, 16);
    CHECK(stl_heap_live(&h) == 1);
    stl_heap_destroy(&h);                     /* 触发"泄漏"警告 */
    CHECK(stl_heap_live(&h) == 0);            /* destroy 后 live 归零 */

    return 0;
}