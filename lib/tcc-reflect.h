/* tcc-reflect.h — 结构体反射的用户侧类型定义
 *
 * 与编译器 __builtin_reflect(Type) 生成的 .rdata 元数据表 ABI 严格对应
 * (见 docs/reflect.md §3): 
 *   __refl        : +0 name(8) +8 kind +12 size +16 align +20 nfield +24 fields(8) = 32B
 *   __refl_field  : +0 name(8) +8 kind +12 offset +16 size +20 align = 24B
 *   kink 编号与 tccgen.c 的 refl_kind() 枚举一致。
 *
 * 用法:
 *   #include "tcc-reflect.h"
 *   const struct __refl *r = (const struct __refl*)__builtin_reflect(struct Vec3);
 *   for (int i = 0; i < (int)r->nfield; i++)
 *       printf("%s @%u %uB align%u\n", r->fields[i].name,
 *              r->fields[i].offset, r->fields[i].size, r->fields[i].align);
 */
#ifndef TCC_REFLECT_H
#define TCC_REFLECT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __refl_kind {
    RE_STRUCT = 1, RE_UNION, RE_PTR, RE_INT, RE_FLOAT, RE_LLONG, RE_BYTE,
    RE_BOOL, RE_ENUM, RE_ARRAY, RE_VOID, RE_SHORT, RE_DOUBLE, RE_LDOUBLE, RE_OTHER
} __refl_kind;

typedef struct __refl_field {
    const char *name;
    unsigned kind;
    unsigned offset;
    unsigned size;
    unsigned align;
} __refl_field;

typedef struct __refl {
    const char *name;
    unsigned kind;
    unsigned size;
    unsigned align;
    unsigned nfield;
    const __refl_field *fields;
} __refl;

#ifdef __cplusplus
}
#endif

#endif /* TCC_REFLECT_H */