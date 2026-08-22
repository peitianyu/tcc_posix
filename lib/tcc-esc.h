/* tcc-esc.h - 内存治理：显式逃逸声明 + 提醒打印 (§6.4)
 *
 * 覆盖「手贱把指针塞进全局再从别处读」这一编译器静态保证不了的真空。
 * 对策不是禁止, 而是让程序员**显式声明**逃逸, 并在 reset/destroy 后若仍有
 * 未撤销的逃逸引用, **醒目打印**, 使其明确知道自己做了什么、后果是什么。
 *
 * 用法（以 arena 为例; 但本表本身与具体分配器解耦, 由调用方喂入"分配纪元"）:
 *
 *   tcc_e0 = a->epoch;
 *   p = tcc_arena_alloc(a, 64, 8);
 *   tcc_esc_register(p, &g_slot, "g_slot", tcc_e0);   // 显式声明逃逸
 *   tcc_arena_reset(a);                                // 纪元前进
 *   tcc_esc_check(a->epoch);                           // 打印仍悬垂的逃逸
 *   tcc_esc_revoke(p, &g_slot);                        // 不再持有的撤销
 *
 * 打印走 stderr, 前缀 [memgov]。全部 static, 无链接依赖, 与 bcheck/memtrack 正交。
 */
#ifndef TCC_ESC_H
#define TCC_ESC_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef TCC_ESC_MAX
#define TCC_ESC_MAX 128u
#endif

typedef struct tcc_esc {
    const void *ptr;         /* 逃逸的指针 */
    const void *holder_slot; /* 持有者(通常 &g_slot) */
    const char *holder;      /* 持有者名字(人类可读) */
    unsigned long epoch;     /* 分配时的纪元(外部提供, 用于判定过期) */
    int  active;             /* 是否仍登记 */
} tcc_esc;

static tcc_esc tcc_esc_g[TCC_ESC_MAX];
static size_t tcc_esc_n;

/* 显式登记一次逃逸: ptr 被写进 holder_slot(符号 holder), 当时分配纪元为 epoch。 */
static void tcc_esc_register(const void *ptr, const void *holder_slot,
                             const char *holder, unsigned long epoch)
{
    size_t i;
    if (tcc_esc_n >= TCC_ESC_MAX) {
        fprintf(stderr, "[memgov] ESC table full, escape %s not tracked\n", holder);
        return;
    }
    for (i = 0; i < tcc_esc_n; i++)
        if (tcc_esc_g[i].active && tcc_esc_g[i].ptr == ptr &&
            tcc_esc_g[i].holder_slot == holder_slot)
            return;                            /* 已登记 */
    tcc_esc_g[tcc_esc_n].ptr = ptr;
    tcc_esc_g[tcc_esc_n].holder_slot = holder_slot;
    tcc_esc_g[tcc_esc_n].holder = holder;
    tcc_esc_g[tcc_esc_n].epoch = epoch;
    tcc_esc_g[tcc_esc_n].active = 1;
    tcc_esc_n++;
}

/* 撤销一次逃逸: 持有者不再持有该指针。 */
static void tcc_esc_revoke(const void *ptr, const void *holder_slot)
{
    size_t i;
    for (i = 0; i < tcc_esc_n; i++)
        if (tcc_esc_g[i].active && tcc_esc_g[i].ptr == ptr &&
            tcc_esc_g[i].holder_slot == holder_slot) {
            tcc_esc_g[i].active = 0;
            return;
        }
}

/* 在 reset/destroy 之后调用: 打印「当前纪元 vs 仍旧登记的逃逸指针」。
   current_epoch 由调用方从分配器读取; 仅当登记纪元 != 当前纪元才提醒。 */
static void tcc_esc_check(unsigned long current_epoch)
{
    size_t i;
    for (i = 0; i < tcc_esc_n; i++)
        if (tcc_esc_g[i].active && tcc_esc_g[i].epoch != current_epoch) {
            fprintf(stderr,
                    "[memgov] ESCALATED pointer %p (epoch %lu -> %lu) "
                    "still referenced by '%s' after reset/destroy\n",
                    tcc_esc_g[i].ptr, tcc_esc_g[i].epoch, current_epoch,
                    tcc_esc_g[i].holder);
            fprintf(stderr,
                    "        WARNING: '%s' now dangles; revoke with "
                    "tcc_esc_revoke if it must outlive this generation, "
                    "or refresh its allocation.\n", tcc_esc_g[i].holder);
        }
}

#endif /* TCC_ESC_H */