/* tcc-stl string.h - STL_string (全功能: SSO + UTF-8 + operator) [M0e→M1]
 *
 * 按 docs/stl.md §7.4 设计:
 *   - **SSO 小串优化**: 短串(≤23B)内联于对象, 免 arena 分配; 长串委托 arena 指针。
 *   - **显式字节/字符双语义**: `size`(字节, O(1)) 与 `length`(UTF-8 字符数, O(N))。
 *   - **operator+ / operator== / operator<**: 字典序/拼接(C++ 直觉写法)。
 *   - 对象 32B 对齐; NUL 结尾; arena 整池回收(无逐对象析构)。
 *
 * 布局(32B):
 *   union { char sso[24]; struct { char *ptr; int len; int cap; } long_; }
 *   int sz;                  -- 总字节数(不含 NUL)
 *   unsigned char mode;      -- 0=SSO, 1=长串
 *   (填充至 32B)
 */
#ifndef STL_STRING_H
#define STL_STRING_H

#include "allocator.h"
#include <string.h>

#define STL_STR_SSO 23                    /* SSO 内联字节上限 */
#define STL_STR_OK   1                    /* 便捷: 非空即真 */

typedef struct STL_string {
    union {
        char    sso[STL_STR_SSO + 1];     /* 短串: sso[0..sz-1] 数据 + [sz] NUL */
        struct { char *ptr; int len, cap; } long_;  /* 长串: cap 含 NUL */
    } u;
    int             sz;                   /* 总字节数(不含 NUL) */
    unsigned char   mode;                 /* 0=SSO, 1=长串 */
} STL_string;

/* ---- 构造 / 查询 ---- */

STL_STATIC STL_string STL_string_new(void)
{
    STL_string s;
    s.mode = 0; s.sz = 0; s.u.sso[0] = 0;
    return s;
}

STL_STATIC int STL_string_empty(const STL_string *s) { return s->sz == 0; }
/* 字节数(O(1)); 不含 NUL */
STL_STATIC int STL_string_size(const STL_string *s) { return s->sz; }

/* 指向 NUL 结尾内容的指针(SSO→内联; 长串→ptr) */
STL_STATIC const char *STL_string_cstr(const STL_string *s)
{
    return s->mode ? s->u.long_.ptr : s->u.sso;
}

/* UTF-8 字符数(O(N)). 忽略无效尾字节; 连续 0 视为 NUL 终止. */
STL_STATIC int STL_string_length(const STL_string *s)
{
    const unsigned char *p = (const unsigned char *)STL_string_cstr(s);
    int bytes = s->sz, n = 0;
    while (bytes > 0) {
        if (*p < 0x80) { p++; bytes--; }
        else {
            int need = 0;
            unsigned int c = *p;
            if ((c & 0xE0) == 0xC0) need = 1;
            else if ((c & 0xF0) == 0xE0) need = 2;
            else if ((c & 0xF8) == 0xF0) need = 3;
            else need = 0;                /* 无效首字节→按 1 计并前进 */
            if (need == 0 || bytes <= need) { p++; bytes--; }
            else { p += 1 + need; bytes -= 1 + need; }
        }
        n++;
    }
    return n;
}

/* 解码 p 处一个 UTF-8 码点; 返回码点值, *adv=前进字节数(无效字节按单字节 ASCII). */
STL_STATIC int STL_string_codepoint(const char *p, int *adv)
{
    unsigned char c = (unsigned char)p[0];
    int cp, n;
    if (c < 0x80)            { cp = c;      n = 1; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
    else                      { cp = c;      n = 1; }
    for (int i = 1; i < n; i++) {
        if (((unsigned char)p[i] & 0xC0) == 0x80)
            cp = (cp << 6) | ((unsigned char)p[i] & 0x3F);
        else { cp = c; n = 1; break; }       /* 续字节无效 → 首字节按原样 */
    }
    if (adv) *adv = n;
    return cp;
}

/* 取第 char_index 个码点值(0 基); 越界返回 -1. 逐码点前进(可与 length 配合迭代). */
STL_STATIC int STL_string_codepoint_at(const STL_string *s, int char_index)
{
    const char *p;
    int bytes;
    if (char_index < 0) return -1;
    p = STL_string_cstr(s);
    bytes = s->sz;
    while (bytes > 0 && char_index > 0) {
        int adv;
        STL_string_codepoint(p, &adv);
        p += adv; bytes -= adv; char_index--;
    }
    if (char_index == 0 && bytes > 0) return STL_string_codepoint(p, 0);
    return -1;
}

/* ---- 预留与追加 (self-contained, arena 可选) ---- */

/* 确保长串容量 ≥ need+1(NUL), 分配自 arena ar. 返回 0=ok, -1=分配失败 */
STL_STATIC int STL_string_reserve(STL_string *s, int need, STL_Arena *ar)
{
    char *np;
    int nc, cap;
    /* 仅长串(mode=1)才含 cap/ptr 字段; SSO 下 union 别名到 sso 内容, 须按 0 处理 */
    cap = s->mode ? s->u.long_.cap : 0;
    if (need + 1 <= cap) return 0;
    nc = cap ? cap * 2 : 16;
    while (nc < need + 1) nc *= 2;
    np = (char *)stl_arena_alloc(ar, (size_t)nc, 1);
    if (!np) return -1;
    memcpy(np, STL_string_cstr(s), s->sz);
    np[s->sz] = 0;
    s->u.long_.ptr = np; s->u.long_.len = s->sz; s->u.long_.cap = nc;
    s->mode = 1;
    return 0;
}

/* 追加 C 串; 优先走 SSO, 溢出则转长串(arena). */
STL_STATIC void STL_string_append_c(STL_string *s, const char *c, STL_Arena *ar)
{
    int cl, nsz;
    cl = c ? (int)strlen(c) : 0;
    if (!cl) return;
    nsz = s->sz + cl;
    if (s->mode == 0 && nsz <= STL_STR_SSO) {
        memcpy(s->u.sso + s->sz, c, (size_t)cl);
        s->sz = nsz; s->u.sso[s->sz] = 0;
        return;
    }
    if (STL_string_reserve(s, nsz, ar)) return;   /* 分配失败保持原样 */
    memcpy(s->u.long_.ptr + s->sz, c, (size_t)cl);
    s->sz = nsz; s->u.long_.ptr[s->sz] = 0;
    s->u.long_.len = s->sz;
}
STL_STATIC void STL_string_append(STL_string *s, const STL_string *o, STL_Arena *ar)
{
    int ol = o ? o->sz : 0;
    if (!ol) return;
    STL_string_append_c(s, STL_string_cstr(o), ar);
}

/* 从 C 串构造(自动 SSO/长串) */
STL_STATIC STL_string STL_string_from_c(const char *c, STL_Arena *ar)
{
    STL_string s = STL_string_new();
    STL_string_append_c(&s, c, ar);
    return s;
}

/* 清空(回到空 SSO) */
STL_STATIC void STL_string_clear(STL_string *s)
{
    s->mode = 0; s->sz = 0; s->u.sso[0] = 0;
}

/* ---- operator 重载 (具体类型, 编译期静态分派) ---- */
int operator==(STL_string a, STL_string b) {
    if (a.sz != b.sz) return 0;
    return a.sz ? memcmp(STL_string_cstr(&a), STL_string_cstr(&b), (size_t)a.sz) == 0 : 1;
}
int operator< (STL_string a, STL_string b) {
    size_t n = a.sz < b.sz ? (size_t)a.sz : (size_t)b.sz;
    int c = n ? memcmp(STL_string_cstr(&a), STL_string_cstr(&b), n) : 0;
    return c < 0 || (c == 0 && a.sz < b.sz);
}

/* 拼接(统一实现): 先放 a 内容再追加 b 内容(SSO 优先, 溢出转长串 arena).
   ar 可为 0(无 arena 时长串分配失败 → 仅短拼接有效, 返回空串). */
STL_STATIC STL_string STL_string_concat(STL_string a, STL_string b, STL_Arena *ar)
{
    STL_string r = a;
    r.sz = 0; r.mode = 0;                /* 复位为空 SSO(丢弃上层长串指针, arena 整池回收) */
    r.u.sso[0] = 0;
    STL_string_append_c(&r, STL_string_cstr(&a), ar);
    STL_string_append_c(&r, STL_string_cstr(&b), ar);
    return r;
}
/* operator+: 委托 STL_string_concat(a,b,0). operator 签名无法携带 arena, 故长串
   (>23B) 拼接会因分配失败返回空串 —— 需长拼接请用 STL_string_concat(a,b,ar) 显式传 arena. */
STL_string operator+ (STL_string a, STL_string b)
{
    return STL_string_concat(a, b, (STL_Arena *)0);
}

#endif /* STL_STRING_H */