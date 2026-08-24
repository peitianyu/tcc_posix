/* t067_stl_checks: M0d - SLT_CHECKS 内在检测层验证 (纯断言, 进程内确定性)
 *
 * 内在检测层(docs/stl.md §4.5-①): SLT_ASSERT(→<assert.h>) 与 allocator 自检
 * (epoch/outstanding 悬挂检测) 在 TCC 原生与 clang 脱糖产物均生效, NDEBUG 裁剪。
 * 本用例在进程内确定性校验检测机制:
 *   1) 正向: SLT_CHECKS 下合法 at/front/back 访问 → 断言编译在且不误伤。
 *   2) allocator 自检: 容器活指针使 outstanding>0(触发 "now dangle" 告警的条件);
 *      slt_arena_reset 清仓(整池回收) 且 epoch 自增; 二次 reset epoch 续增。
 *      (注: SLT_ASSERT 的 abort 路径在本 Windows nt64 musl 端口因 raise(SIGABRT)
 *       无效而挂死, 越界 at() 的 "Assertion failed" 输出已在单独探针验证,
 *       见 docs/KNOWN_ISSUES.md。)
 * 退出码 0 = 通过.
 */
#include "lib/stl/vector.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    SLT_Arena *ar = slt_arena_new(0);
    CHECK(ar != 0);
    CHECK(ar->epoch == 0 && ar->outstanding == 0);

    /* 1) 正向 —— 断言开启下合法访问不误伤 */
    {
        Vector(int) v; Vector_init(int)(&v, ar);
        for (int i = 0; i < 20; i++) Vector_push_back(int)(&v, i * 5);
        CHECK(Vector_at(int)(&v, 0) == 0);
        CHECK(Vector_at(int)(&v, 10) == 50);
        CHECK(Vector_at(int)(&v, 19) == 95);
        CHECK(Vector_front(int)(&v) == 0);
        CHECK(Vector_back(int)(&v) == 95);
    }

    /* 2) allocator 自检 —— 活指针被追踪, reset 清仓 + epoch 自增 */
    {
        Vector(int) v; Vector_init(int)(&v, ar);
        for (int i = 0; i < 3; i++) Vector_push_back(int)(&v, i);
        /* 活指针使 outstanding>0 —— 正是 reset/destroy 报 "now dangle" 的条件 */
        CHECK(ar->outstanding >= 1);

        slt_arena_reset(ar);                 /* 整池回卷 + epoch++ */
        CHECK(ar->outstanding == 0);
        CHECK(ar->epoch == 1);

        /* 二次 reset: epoch 续增(epoch 单调, 供陈旧指针 epoch 比对) */
        slt_arena_reset(ar);
        CHECK(ar->epoch == 2);
    }

    slt_arena_destroy(ar);
    return 0;
}