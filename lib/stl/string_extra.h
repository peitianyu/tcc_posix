/* tcc-stl string_extra.h - STL_string 自由函数 (§7.4 C7: 复用 arena 免临时分配)
 *
 *   stl_string_split(s, sep, out[], max, ar): 按字节分隔符切段
 *   stl_string_trim(s, ar):                   去首尾空白(空格 \\t \\n \\r \\v \\f)
 *   stl_string_join(parts, n, sep, ar):       用分隔符拼接
 *
 * 与 string.h 同翻译单元编译(include 展开), 直接复用其 STL_STATIC 内部函数
 * (stl_string_reserve/append_c 等); 段/结果优先 SSO, 溢出经 arena。
 */
#ifndef STL_STRING_EXTRA_H
#define STL_STRING_EXTRA_H

#include "string.h"

/* 从 [start,start+len) 构造子串(非 NUL 结尾源): 短段 SSO, 长段 arena 一次性拷贝 */
STL_STATIC STL_string stl_string_from_range(const char *start, int len, STL_Arena *ar)
{
    STL_string r = stl_string_new();
    if (len <= STL_STR_SSO) {
        memcpy(r.u.sso, start, (size_t)len);
        r.u.sso[len] = 0;
        r.sz = len;
        r.mode = 0;
    } else if (!stl_string_reserve(&r, len, ar)) {
        memcpy(r.u.long_.ptr, start, (size_t)len);
        r.u.long_.ptr[len] = 0;
        r.sz = len;
        r.u.long_.len = len;
    }
    return r;
}

/* 按字节分隔符 sep 切段: 段写入 out[0..min(段数,max)-1], 返回总段数。
 * 空段计入(连续分隔符/首尾分隔符产生空段); 段数 > max 时只填前 max 段。 */
STL_STATIC int stl_string_split(const STL_string *s, char sep,
                                STL_string *out, int max, STL_Arena *ar)
{
    const char *p = stl_string_cstr(s);
    const char *end = p + s->sz;
    const char *start = p;
    int n = 0;
    for (const char *q = p; q <= end; q++) {
        if (q == end || *q == sep) {
            if (n < max)
                out[n] = stl_string_from_range(start, (int)(q - start), ar);
            n++;
            start = q + 1;
        }
    }
    return n;
}

/* 去首尾空白(空格 \\t \\n \\r \\v \\f), 返回新串(SSO/长串) */
STL_STATIC STL_string stl_string_trim(const STL_string *s, STL_Arena *ar)
{
    const char *p = stl_string_cstr(s);
    const char *end = p + s->sz;
    const char *b = p, *e = end;
    while (b < end) {
        unsigned char c = (unsigned char)*b;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f')
            b++;
        else
            break;
    }
    while (e > b) {
        unsigned char c = (unsigned char)e[-1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f')
            e--;
        else
            break;
    }
    return stl_string_from_range(b, (int)(e - b), ar);
}

/* 用分隔符 sep 拼接 parts[0..n-1], 返回新串(总长一次 reserve) */
STL_STATIC STL_string stl_string_join(const STL_string *parts, int n,
                                      const char *sep, STL_Arena *ar)
{
    STL_string r = stl_string_new();
    int sepl = sep ? (int)strlen(sep) : 0;
    int total = 0;
    for (int i = 0; i < n; i++)
        total += parts[i].sz + (i ? sepl : 0);
    if (total <= STL_STR_SSO) {
        for (int i = 0; i < n; i++) {
            if (i && sepl) stl_string_append_c(&r, sep, ar);
            stl_string_append_c(&r, stl_string_cstr(&parts[i]), ar);
        }
        return r;
    }
    if (stl_string_reserve(&r, total, ar)) return r;   /* 分配失败返回空串 */
    for (int i = 0; i < n; i++) {
        if (i && sepl) {
            memcpy(r.u.long_.ptr + r.sz, sep, (size_t)sepl);
            r.sz += sepl;
        }
        memcpy(r.u.long_.ptr + r.sz, stl_string_cstr(&parts[i]), (size_t)parts[i].sz);
        r.sz += parts[i].sz;
    }
    r.u.long_.ptr[r.sz] = 0;
    r.u.long_.len = r.sz;
    return r;
}

#endif /* STL_STRING_EXTRA_H */
