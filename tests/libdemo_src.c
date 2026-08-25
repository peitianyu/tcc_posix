/* libdemo_src.c — emit-c 独立库导出场景验收源 (无 main)
 * 混合 model 泛型 + operator + reflect + defer 的库模块:
 *   - 公开 API: vec_add / vec_make (导出)
 *   - 内部机制: model 实例函数 / operator 定义 / 反射表 (应 static)
 * 脱糖 → clang -flto -fvisibility=hidden 编 .o → 归档 → 消费端链接调用. */
#include <stdio.h>
#include "cpu-prof.h"      /* 仅验证 include 保真, 不插桩 */
#include "tcc-reflect.h"
#include <string.h>

/* operator 定义 (非 static → 导出; 库内自用) */
struct Pt { int x, y; };
struct Pt operator+ (struct Pt a, struct Pt b) { struct Pt r = { a.x + b.x, a.y + b.y }; return r; }
int operator< (struct Pt a, struct Pt b) { return a.x < b.x; }

/* model 泛型: 实例化函数应为 static */
model struct Box(T) { T v; };
model (T) void box_set(Box(T) *b, T v) { b->v = v; }

/* 公开 API */
struct Pt vec_add(struct Pt a, struct Pt b) { return a + b; }
int vec_cmp(struct Pt a, struct Pt b) { return a < b; }

Box(int) *box_new_int(void)
{
    static Box(int) b;
    box_set(int)(&b, 42);
    return &b;
}

int box_val(Box(int) *b) { return b->v; }

/* reflect 自检: 库内用反射拿字段信息 (表应 static, 不导出) */
int pt_fields(void)
{
    const struct __refl *r = (const struct __refl *)__builtin_reflect(struct Pt);
    return (int)r->nfield;
}

/* defer 用例 */
void deferred_work(int *out)
{
    FILE *f = fopen("nul", "w");
    if (!f) { *out = -1; return; }
    defer fclose(f);
    fprintf(f, "x");
    *out = 7;
}
