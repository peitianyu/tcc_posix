/* t078_stl_string_extra: §7.4 C7 - string 自由函数 split/trim/join (纯断言)
 * 覆盖: split(普通/连续分隔符/首尾分隔符/长段溢出 SSO/截断 max)、
 *       trim(首尾/全空白/无空白)、join(分隔符/空数组/长结果)。
 * 退出码 0 = 通过.
 */
#include "lib/stl/string_extra.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void)
{
    STL_Arena *ar = stl_arena_new(0);

    /* 1. split 基础: "a,b,c" → 3 段 */
    {
        STL_string s = stl_string_from_c("a,b,c", ar);
        STL_string out[8];
        int n = stl_string_split(&s, ',', out, 8, ar);
        CHECK(n == 3);
        CHECK(stl_string_size(&out[0]) == 1 && stl_string_cstr(&out[0])[0] == 'a');
        CHECK(stl_string_cstr(&out[1])[0] == 'b');
        CHECK(stl_string_cstr(&out[2])[0] == 'c');
    }

    /* 2. 连续/首尾分隔符 → 空段 */
    {
        STL_string s = stl_string_from_c(",a,,b,", ar);
        STL_string out[8];
        int n = stl_string_split(&s, ',', out, 8, ar);
        CHECK(n == 5);
        CHECK(stl_string_size(&out[0]) == 0);      /* 空段 */
        CHECK(stl_string_cstr(&out[1])[0] == 'a');
        CHECK(stl_string_size(&out[2]) == 0);
        CHECK(stl_string_cstr(&out[3])[0] == 'b');
        CHECK(stl_string_size(&out[4]) == 0);      /* 尾分隔符 → 尾空段 */
    }

    /* 3. 长段(>23B)溢出 SSO → 长串经 arena; 内容完整 */
    {
        const char *big = "0123456789abcdefghijklmnopqrstuvwxyz";  /* 36B */
        STL_string s = stl_string_from_c(big, ar);
        STL_string out[4];
        int n = stl_string_split(&s, '-', out, 4, ar);   /* 无分隔符 → 整串一段 */
        CHECK(n == 1);
        CHECK(stl_string_size(&out[0]) == 36);
        CHECK(memcmp(stl_string_cstr(&out[0]), big, 36) == 0);
    }

    /* 4. max 截断: 只填前 max 段, 返回总数 */
    {
        STL_string s = stl_string_from_c("x:y:z:w", ar);
        STL_string out[2];
        int n = stl_string_split(&s, ':', out, 2, ar);
        CHECK(n == 4);
        CHECK(stl_string_cstr(&out[0])[0] == 'x');
        CHECK(stl_string_cstr(&out[1])[0] == 'y');
    }

    /* 5. trim 基础 */
    {
        STL_string s = stl_string_from_c("  \t hello \n ", ar);
        STL_string t = stl_string_trim(&s, ar);
        CHECK(stl_string_size(&t) == 5);
        CHECK(memcmp(stl_string_cstr(&t), "hello", 5) == 0);
    }

    /* 6. trim 全空白 / 无空白 */
    {
        STL_string s1 = stl_string_from_c(" \t\r\n ", ar);
        STL_string t1 = stl_string_trim(&s1, ar);
        CHECK(stl_string_size(&t1) == 0);

        STL_string s2 = stl_string_from_c("keep", ar);
        STL_string t2 = stl_string_trim(&s2, ar);
        CHECK(stl_string_size(&t2) == 4);
        CHECK(memcmp(stl_string_cstr(&t2), "keep", 4) == 0);
    }

    /* 7. trim 长串(前后空白 + 中间长内容) */
    {
        const char *inner = "0123456789abcdefghijklmnopqrstuvwxyz";
        char buf[80];
        STL_string s, t;
        snprintf(buf, sizeof buf, "  \t%s \n", inner);
        s = stl_string_from_c(buf, ar);
        t = stl_string_trim(&s, ar);
        CHECK(stl_string_size(&t) == 36);
        CHECK(memcmp(stl_string_cstr(&t), inner, 36) == 0);
    }

    /* 8. join 基础 + 空数组 */
    {
        STL_string parts[3];
        parts[0] = stl_string_from_c("aa", ar);
        parts[1] = stl_string_from_c("b", ar);
        parts[2] = stl_string_from_c("ccc", ar);
        STL_string j = stl_string_join(parts, 3, "-", ar);
        CHECK(stl_string_size(&j) == 8);                 /* "aa"+"-"+"b"+"-"+"ccc" */
        CHECK(memcmp(stl_string_cstr(&j), "aa-b-ccc", 8) == 0);   /* 含 NUL */

        STL_string j0 = stl_string_join(parts, 0, "-", ar);
        CHECK(stl_string_size(&j0) == 0);
    }

    /* 9. join 长结果(总长 > SSO) */
    {
        STL_string parts[3];
        const char *b1 = "0123456789abcdef";   /* 16B */
        const char *b2 = "ghijklmnopqrstuv";   /* 16B */
        parts[0] = stl_string_from_c(b1, ar);
        parts[1] = stl_string_from_c(b2, ar);
        parts[2] = stl_string_from_c("xyz", ar);
        STL_string j = stl_string_join(parts, 3, "--", ar);
        CHECK(stl_string_size(&j) == 16 + 16 + 3 + 2 * 2);   /* 39B > 23 */
        CHECK(memcmp(stl_string_cstr(&j), "0123456789abcdef--ghijklmnopqrstuv--xyz", 40) == 0);
    }

    stl_arena_destroy(ar);
    return 0;
}
