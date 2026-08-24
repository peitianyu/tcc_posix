/* tcc-stl string.h - STL_string (基础版, arena 后端) [M0e 先行]
 *
 * 设计底座参考 docs/stl.md §7.4, 本文件交付 M0e 基础片:
 *   - 具体类型(非 model)——复用 operator 扩展(operator+ / operator== / operator<
 *     的 C++ 风格运算符糖)拼串与字典序比较。
 *   - 存储自 arena(STL_Arena, self-contained, 仅 musl 头), 整池回收, 无逐对象析构。
 *   - 双字段字节语义: `len`=字节数(O(1)); `length`=字符串长度(纯字节串即 len)。
 *   - 以 NUL 结尾(ctor/reserve 预留末尾 0), c_str() 安全。
 *
 * 未做(记为后续): SSO 内联缓冲 23B、长串 ptr 字段、UTF-8 码点迭代(string_length
 * O(N))、heap 后端、脱糖 dg_op_tbl 同步 —— 见 docs/stl.md §7.4。
 * 约束: operator 为编译期静态分派, 仅结构体值类型可重载; + / == / < 映射到
 *   operator+ / operator== / operator< 名(与 features.md §4.4 同款)。
 */
#ifndef STL_STRING_H
#define STL_STRING_H

#include "allocator.h"
#include <string.h>

typedef struct STL_string {
    char     *ptr;          /* 内容(arena), NUL 结尾; 空串为 0 */
    size_t    len;          /* 字节数(不含 NUL) */
    size_t    cap;          /* 容量(含末尾 NUL 位) */
    STL_Arena *ar;          /* 分配自哪个 arena */
} STL_string;

STL_STATIC STL_string stl_string_new(STL_Arena *ar)
{
    STL_string s;
    s.ptr = 0; s.len = 0; s.cap = 0; s.ar = ar;
    return s;
}

/* 确保容量 ≥ need+1(NUL). 复用同 arena。 */
STL_STATIC void stl_string_reserve(STL_string *s, size_t need)
{
    size_t nc;
    char *np;
    if (need + 1 <= s->cap) return;
    nc = s->cap ? s->cap : 16;
    while (nc < need + 1) nc *= 2;
    np = (char *)stl_arena_alloc(s->ar, nc, 1);
    if (!np) return;
    if (s->len && s->ptr) memcpy(np, s->ptr, s->len);
    np[s->len] = 0;
    s->ptr = np; s->cap = nc;
}

STL_STATIC size_t stl_string_length(const STL_string *s) { return s->len; }
STL_STATIC int   stl_string_empty (const STL_string *s) { return s->len == 0; }
STL_STATIC const char *stl_string_cstr(const STL_string *s) { return s->ptr ? s->ptr : ""; }

STL_STATIC void stl_string_clear(STL_string *s) { s->len = 0; if (s->ptr) s->ptr[0] = 0; }

/* 追加 C 串 / 另一 STL_string (字节拷贝) */
STL_STATIC void stl_string_append_c(STL_string *s, const char *c)
{
    size_t cl = c ? strlen(c) : 0;
    if (!cl) return;
    stl_string_reserve(s, s->len + cl);
    if (!s->ptr) return;
    memcpy(s->ptr + s->len, c, cl);
    s->len += cl;
    s->ptr[s->len] = 0;
}
STL_STATIC void stl_string_append(STL_string *s, const STL_string *o)
{
    if (!o || !o->len) return;
    stl_string_reserve(s, s->len + o->len);
    if (!s->ptr) return;
    memcpy(s->ptr + s->len, o->ptr, o->len);
    s->len += o->len;
    s->ptr[s->len] = 0;
}

/* tuple 式初值: 如 STL_string a = stl_string_new(ar) 后赋值 */
STL_STATIC STL_string stl_string_from_c(STL_Arena *ar, const char *c)
{
    STL_string s = stl_string_new(ar);
    stl_string_append_c(&s, c);
    return s;
}

/* ---- operator 重载 (具体类型, 编译期静态分派) ---- */
int operator==(STL_string a, STL_string b) {
    if (a.len != b.len) return 0;
    return a.len ? memcmp(a.ptr, b.ptr, a.len) == 0 : 1;
}
int operator<(STL_string a, STL_string b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int c = n ? memcmp(a.ptr, b.ptr, n) : 0;
    return c < 0 || (c == 0 && a.len < b.len);
}
/* a+b 拼接, 结果分配进 a 的 arena, 按值返回(与原操作数独立) */
STL_string operator+ (STL_string a, STL_string b) {
    STL_string r = stl_string_new(a.ar);
    stl_string_append(&r, &a);
    stl_string_append(&r, &b);
    return r;
}

#endif /* STL_STRING_H */