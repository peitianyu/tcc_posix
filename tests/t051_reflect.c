/* t051_reflect.c — 结构体反射 (__builtin_reflect) 验收
 *
 * 断言反射元数据的 nfield/size 与 sizeof 一致, 每字段 name/offset/size 与
 * offsetof/sizeof 一致, 遍历驱动"按元数据访问"。退出码 0 = 通过。
 * 构建: bin/tcc.exe tests/t051_reflect.c -o t051_reflect.exe
 */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "tcc-reflect.h"

struct Vec3 { float x, y, z; };
struct Mixed { char c; int i; double d; short s; };

int main(void)
{
    const struct __refl *v3 = (const struct __refl *)__builtin_reflect(struct Vec3);
    const struct __refl *mx = (const struct __refl *)__builtin_reflect(struct Mixed);
    int fail = 0;

    if (!v3 || !mx) { puts("FAIL: null reflect"); return 1; }

    /* Vec3 header */
    if (v3->nfield != 3) { printf("FAIL: Vec3 nfield=%u\n", v3->nfield); fail = 1; }
    if (v3->size != sizeof(struct Vec3)) { printf("FAIL: Vec3 size=%u\n", v3->size); fail = 1; }
    if (v3->align != _Alignof(struct Vec3)) { printf("FAIL: Vec3 align=%u\n", v3->align); fail = 1; }

    /* Vec3 字段 */
    {
        const struct __refl_field *f = v3->fields;
        if (strcmp(f[0].name, "x") || strcmp(f[1].name, "y") || strcmp(f[2].name, "z")) { puts("FAIL: Vec3 names"); fail = 1; }
        if (f[0].offset != offsetof(struct Vec3, x) || f[1].offset != offsetof(struct Vec3, y)
            || f[2].offset != offsetof(struct Vec3, z)) { puts("FAIL: Vec3 offsets"); fail = 1; }
        if (f[0].size != 4 || f[1].size != 4 || f[2].size != 4) { puts("FAIL: Vec3 fsize"); fail = 1; }
        if (f[0].align != 4 || f[2].align != 4) { puts("FAIL: Vec3 falign"); fail = 1; }
        if (f[0].kind != RE_FLOAT) { puts("FAIL: Vec3.x kind"); fail = 1; }
    }

    /* Mixed 字段 (混排对齐) */
    {
        const struct __refl_field *f = mx->fields;
        if (mx->nfield != 4) { printf("FAIL: Mixed nfield=%u\n", mx->nfield); fail = 1; }
        if (strcmp(f[0].name, "c") || strcmp(f[1].name, "i") || strcmp(f[2].name, "d") || strcmp(f[3].name, "s")) { puts("FAIL: Mixed names"); fail = 1; }
        if (f[0].offset != offsetof(struct Mixed, c) || f[1].offset != offsetof(struct Mixed, i)
            || f[2].offset != offsetof(struct Mixed, d) || f[3].offset != offsetof(struct Mixed, s)) { puts("FAIL: Mixed offsets"); fail = 1; }
        if (f[0].size != 1 || f[1].size != 4 || f[2].size != 8 || f[3].size != 2) { puts("FAIL: Mixed sizes"); fail = 1; }
    }

    /* 遍历驱动: 按元数据把 Vec3 逐字段码进字节流 (假序列化触发 ) */
    {
        unsigned char buf[16];
        int off = 0, i;
        const struct __refl_field *f = v3->fields;
        struct Vec3 a = { 1.f, 2.f, 3.f };
        memset(buf, 0, sizeof buf);
        for (i = 0; i < (int)v3->nfield; i++)
            memcpy(buf + f[i].offset, (char *)&a + f[i].offset, f[i].size);
        if (*(float *)buf != 1.f || *(float *)(buf + 4) != 2.f) { puts("FAIL: reflect-driven copy"); fail = 1; }
        (void)off;
    }

    if (fail) { puts("FAIL: t051_reflect"); return 1; }
    puts("PASS: t051_reflect");
    return 0;
}