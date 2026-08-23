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
struct Node { struct Vec3 v; int tag; };
struct Arr { int vals[4]; struct Vec3 tri[2]; };
enum Kind { K_A = 1, K_B = 2 };
struct Ptr { int *p; int n; };
struct En { enum Kind k; int x; };

/* P2: 通用按反射递归深拷贝 (嵌套经 sub; 数组元素为 struct 时逐元素递归) */
static void refl_copy(char *dst, const char *src, const struct __refl *r)
{
    int i;
    for (i = 0; i < (int)r->nfield; i++) {
        const struct __refl_field *f = r->fields + i;
        if (f->sub && f->count > 1) {           /* 数组元素是 struct: 逐元素递归 */
            unsigned esz = f->size / f->count;
            int k;
            for (k = 0; k < (int)f->count; k++)
                refl_copy(dst + f->offset + k * esz, src + f->offset + k * esz, f->sub);
        } else if (f->sub) {                    /* 单值 struct */
            refl_copy(dst + f->offset, src + f->offset, f->sub);
        } else {
            memcpy(dst + f->offset, src + f->offset, f->size);
        }
    }
}

int main(void)
{
    const struct __refl *v3 = (const struct __refl *)__builtin_reflect(struct Vec3);
    const struct __refl *mx = (const struct __refl *)__builtin_reflect(struct Mixed);
    int fail = 0;

    if (!v3 || !mx) { puts("FAIL: null reflect"); return 1; }

    /* 缓存: 同类型多次 __builtin_reflect 复用同一张表 (同地址) */
    if (v3 != (const struct __refl *)__builtin_reflect(struct Vec3)
        || mx != (const struct __refl *)__builtin_reflect(struct Mixed)) {
        puts("FAIL: reflect cache reuse"); fail = 1;
    }

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

    /* P1: 嵌套 struct 值字段 sub 指向子表 */
    {
        const struct __refl *nd = (const struct __refl *)__builtin_reflect(struct Node);
        const struct __refl_field *fv, *ft;
        if (nd->nfield != 2) { puts("FAIL: Node nfield"); fail = 1; }
        fv = &nd->fields[0];
        ft = &nd->fields[1];
        if (!fv->sub || fv->sub->nfield != 3 || fv->sub != v3) { puts("FAIL: Node.v sub"); fail = 1; }
        if (ft->offset != offsetof(struct Node, tag)) { puts("FAIL: Node.tag offset"); fail = 1; }
        if (ft->sub != NULL) { puts("FAIL: Node.tag sub null"); fail = 1; }
    }

    /* P2: 通用按反射递归深拷贝 (嵌套经 sub) */
    {
        struct Node a = { { 4.f, 5.f, 6.f }, 42 };
        struct Node b;
        const struct __refl *nd = (const struct __refl *)__builtin_reflect(struct Node);
        memset(&b, 0, sizeof b);
        refl_copy((char *)&b, (const char *)&a, nd);
        if (b.v.x != 4.f || b.v.z != 6.f || b.tag != 42) { puts("FAIL: refl deep copy"); fail = 1; }
    }

    /* 数组字段: count + 数组元素 struct 的 sub + 深拷贝 */
    {
        const struct __refl *ar = (const struct __refl *)__builtin_reflect(struct Arr);
        const struct __refl_field *fv = &ar->fields[0];
        const struct __refl_field *ft = &ar->fields[1];
        if (ar->nfield != 2) { puts("FAIL: Arr nfield"); fail = 1; }
        if (fv->kind != RE_ARRAY || fv->count != 4 || fv->size != 16 || fv->sub != NULL) { puts("FAIL: Arr vals"); fail = 1; }
        if (ft->kind != RE_ARRAY || ft->count != 2 || ft->size != 24 || !ft->sub || ft->sub != v3) { puts("FAIL: Arr tri"); fail = 1; }
        /* 数组深拷贝 (ints 整体 + Vec3[2] 逐元素) */
        {
            struct Arr a = { { 1, 2, 3, 4 }, { { 9.f, 8.f, 7.f }, { 6.f, 5.f, 4.f } } };
            struct Arr b;
            memset(&b, 0, sizeof b);
            refl_copy((char *)&b, (const char *)&a, ar);
            if (b.vals[3] != 4 || b.tri[1].z != 4.f || b.tri[0].y != 8.f) { puts("FAIL: refl array copy"); fail = 1; }
        }
    }

    /* 指针 + enum 字段 kind/尺寸 */
    {
        const struct __refl *np = (const struct __refl *)__builtin_reflect(struct Ptr);
        const struct __refl_field *fp = &np->fields[0];
        const struct __refl *ne = (const struct __refl *)__builtin_reflect(struct En);
        const struct __refl_field *fe = &ne->fields[0];
        if (fp->kind != RE_PTR || fp->size != 8 || fp->align != 8 || fp->sub != NULL) { puts("FAIL: Ptr.p kind"); fail = 1; }
        if (fe->kind != RE_ENUM || fe->size != 4 || fe->sub != NULL) { puts("FAIL: En.k enum"); fail = 1; }
        if (((const struct __refl *)__builtin_reflect(enum Kind))->kind != RE_ENUM) { puts("FAIL: Kind enum kind"); fail = 1; }
    }

    if (fail) { puts("FAIL: t051_reflect"); return 1; }
    puts("PASS: t051_reflect");
    return 0;
}