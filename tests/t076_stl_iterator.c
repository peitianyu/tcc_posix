/* t076_stl_iterator: M1-待办 - 抽象迭代器算法骨架 + list 算法中断 (纯断言)
 *
 * Part A (通用抽象迭代器): 手写一个 int 双链, 实例化 per-int 的 `stl_iter_ops`
 *   方法表(incr/deref/eq), 填 `STL_Iter(int)` begin/end 对象, 让通用泛型算法
 *   `stl_iter_find/count/for_each`(iterator.h) 经 vptr **间接调用**遍历它——
 *   证明算法与具体容器布局解耦, 是"抽象迭代器算法骨架"的核心。
 * Part B (list 算法中断): 用 STL_List(int) 的 `stl_list_find/count/for_each`
 *   对链式容器做同语义遍历(内联, 避免泛型 per-T 静态表重复定义)。
 *
 * 调用风格: 泛型自由算法 `stl_iter_find(int)(beg,end,val)`; list 容器对象方法糖
 * `l->stl_list_find(int)(v)`。
 * 退出码 0 = 通过.
 */
#include "lib/stl/iterator.h"
#include "lib/stl/list.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* ---- Part A: per-int 抽象迭代器(演示用 int 双链) ---- */
struct inode { struct inode *next; int data; };
static struct inode nodes[8];

static void   i_incr(void *z) { STL_Iter(int) *it = (STL_Iter(int) *)z; it->ctx = ((struct inode *)it->ctx)->next; }
static void  *i_deref(void *z) { STL_Iter(int) *it = (STL_Iter(int) *)z; return it->ctx ? &((struct inode *)it->ctx)->data : 0; }
static int    i_eq   (const void *a, const void *b) {
    return ((const STL_Iter(int) *)a)->ctx == ((const STL_Iter(int) *)b)->ctx;
}
static const stl_iter_ops i_ops = { i_incr, i_deref, i_eq };

/* Part B for_each 回调(fn(T*) 无 ud → 用全局累加) */
static int g_sum;
static void lacc(int *p) { g_sum += *p; }

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* ---- Part A: 抽象迭代器路径 ---- */
    {
        STL_Iter(int) beg, end, cur, it;
        for (int i = 0; i < 5; i++) {
            nodes[i].next = (i < 4 ? &nodes[i + 1] : 0);
            nodes[i].data = (i + 1) * 10;
        }
        STL_ITER_SET(beg, &i_ops, &nodes[0]);
        STL_ITER_SET(end, &i_ops, 0);

        /* find: 命中返回地址; 未命中 0 */
        int *f = stl_iter_find(int)(beg, end, 30);
        CHECK(f && *f == 30);
        CHECK(stl_iter_find(int)(beg, end, 99) == 0);
        CHECK(stl_iter_find(int)(end, end, 10) == 0);   /* 空区间 */

        /* count */
        CHECK(stl_iter_count(int)(beg, end, 10) == 1);
        CHECK(stl_iter_count(int)(beg, end, 60) == 0);

        /* for_each: 经 vptr 累加(与裸指针 stl_for_each 同语义, 只经间接调用) */
        g_sum = 0;
        stl_iter_for_each(int)(beg, end, lacc);
        CHECK(g_sum == 150);

        /* 值语义: 复制推进不互相影响 */
        cur = beg; cur.ops->incr(&cur); cur.ops->incr(&cur);
        it = beg;
        CHECK(beg.ctx == &nodes[0]);                   /* beg 未被 cur 修改 */
        CHECK(cur.ctx == &nodes[2]);
        CHECK(it.ctx == &nodes[0]);
    }

    /* ---- Part B: list 算法中断 ---- */
    {
        STL_List(int) l; l->stl_list_init(int)(ar);
        for (int i = 0; i < 50; i++) l->stl_list_push_back(int)(i % 5);   /* 各值恰 10 次 */
        CHECK(l->stl_list_size(int)() == 50);

        CHECK(*l->stl_list_find(int)(3) == 3);         /* 首个 3 命中 */
        CHECK(l->stl_list_find(int)(7) == 0);          /* 无 7 */
        CHECK(l->stl_list_count(int)(0) == 10);
        CHECK(l->stl_list_count(int)(4) == 10);

        g_sum = 0;
        l->stl_list_for_each(int)(lacc);
        CHECK(g_sum == (0 + 1 + 2 + 3 + 4) * 10);
    }

    stl_arena_destroy(ar);
    return 0;
}