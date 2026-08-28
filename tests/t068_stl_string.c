/* t068_stl_string: M0e→M1 - STL_string 全功能 (纯断言)
 * 覆盖: SSO 短串(免分配/mode=0)、长串(转 arena)、size(字节) vs length(UTF-8 字符)、
 *       cstr/empty/clear、operator+ / operator== / operator<、stl_string_concat。
 * 退出码 0 = 通过.
 */
#include "lib/stl/string.h"
#include <string.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. SSO 短串: 免分配, 字节/字符计数 */
    {
        STL_string s = stl_string_new();
        CHECK(stl_string_empty(&s));
        stl_string_append_c(&s, "Hello, ", ar);
        stl_string_append_c(&s, "World!", ar);
        CHECK(stl_string_size(&s) == 13);            /* 13 字节 */
        CHECK(!s.mode);                              /* ≤23B → SSO 内联 */
        CHECK(strcmp(stl_string_cstr(&s), "Hello, World!") == 0);

        stl_string_clear(&s);
        CHECK(stl_string_empty(&s));
        stl_string_append_c(&s, "again", ar);
        CHECK(stl_string_size(&s) == 5);
        CHECK(strcmp(stl_string_cstr(&s), "again") == 0);
    }

    /* 2. 长串(>23B): 转 arena,mode=1,size 字节   */
    {
        STL_string s = stl_string_from_c("0123456789abcdefghijklmn", ar);  /* 24B */
        CHECK(stl_string_size(&s) == 24);
        CHECK(s.mode == 1);                          /* 溢出 SSO → 长串 */
        CHECK(strcmp(stl_string_cstr(&s), "0123456789abcdefghijklmn") == 0);
    }

    /* 3. UTF-8: size=字节, length=字符数, 逐码点迭代 */
    {
        STL_string s = stl_string_from_c("你好 Aa", ar);  /* 你3+好3+空格1+A1+a1 = 9B; 5 字符 */
        int size = stl_string_size(&s), len = stl_string_length(&s);
        CHECK(size == 9);
        CHECK(len == 5);
        /* 码点: 你=U+4F60, 好=U+597D, ' '=0x20, 'A'=0x41, 'a'=0x61 */
        CHECK(stl_string_codepoint_at(&s, 0) == 0x4F60);
        CHECK(stl_string_codepoint_at(&s, 1) == 0x597D);
        CHECK(stl_string_codepoint_at(&s, 2) == 0x20);
        CHECK(stl_string_codepoint_at(&s, 3) == 0x41);
        CHECK(stl_string_codepoint_at(&s, 4) == 0x61);
        CHECK(stl_string_codepoint_at(&s, 5) == -1);      /* 越界 */
        /* ASCII: 1 字节前进 */
        {
            int adv;
            CHECK(stl_string_codepoint("A", &adv) == 0x41 && adv == 1);
        }
    }

    /* 4. operator+ (SSO 内拼接) 与 stl_string_concat (长拼接) */
    {
        STL_string a = stl_string_from_c("foo", ar);
        STL_string b = stl_string_from_c("bar", ar);
        STL_string ab = a + b;                                  /* SSO:"foobar" */
        CHECK(strcmp(stl_string_cstr(&ab), "foobar") == 0);
        CHECK(stl_string_size(&ab) == 6);

        /* 长拼接(超 SSO)显式传 arena */
        STL_string c = stl_string_from_c("The quick brown fox ", ar);
        STL_string d = stl_string_from_c("jumps over the lazy dog", ar);  /* 总 > 23B */
        STL_string cd = stl_string_concat(c, d, ar);
        CHECK(stl_string_size(&cd) == 43);
        CHECK(memcmp(stl_string_cstr(&cd), "The quick brown fox jumps over the lazy dog", 43) == 0);
    }

    /* 5. operator== / operator< (字典序) */
    {
        STL_string x = stl_string_from_c("apple", ar);
        STL_string same = stl_string_from_c("apple", ar);
        STL_string y = stl_string_from_c("banana", ar);
        STL_string pfx = stl_string_from_c("app", ar);
        CHECK(x == same);
        CHECK(!(x == y));
        CHECK(x < y);
        CHECK(!(y < x));
        CHECK(pfx < x);
        CHECK(x == x);
    }

    stl_arena_destroy(ar);
    return 0;
}