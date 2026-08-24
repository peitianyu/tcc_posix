/* t068_stl_string: M0e - STL_string 基础版 (纯断言)
 * 覆盖: 构建/append/c_str/length/clear, 拼接 (operator+), 字典序比较
 *       (operator<) 与判等 (operator==), 复用同一 arena 多字符串不串扰。
 * 退出码 0 = 通过.
 */
#include "lib/stl/string.h"
#include <string.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 构建 + append + c_str */
    {
        STL_string s = stl_string_new(ar);
        CHECK(stl_string_empty(&s));
        stl_string_append_c(&s, "Hello");
        stl_string_append_c(&s, ", ");
        stl_string_append_c(&s, "World!");
        CHECK(stl_string_length(&s) == 13);
        CHECK(strcmp(stl_string_cstr(&s), "Hello, World!") == 0);

        stl_string_clear(&s);
        CHECK(stl_string_empty(&s));
        stl_string_append_c(&s, "again");
        CHECK(stl_string_length(&s) == 5);
        CHECK(strcmp(stl_string_cstr(&s), "again") == 0);
    }

    /* 拼接 operator+ 与 append(STL_string) */
    {
        STL_string a = stl_string_from_c(ar, "foo");
        STL_string b = stl_string_from_c(ar, "bar");
        STL_string ab = a + b;                       /* operator+ : "foobar" */
        CHECK(strcmp(stl_string_cstr(&ab), "foobar") == 0);
        CHECK(stl_string_length(&ab) == 6);

        STL_string c = stl_string_new(ar);
        stl_string_append(&c, &a);                    /* "foo" */
        stl_string_append_c(&c, "-");
        stl_string_append(&c, &b);                    /* "foo-bar" */
        CHECK(strcmp(stl_string_cstr(&c), "foo-bar") == 0);
    }

    /* operator== / operator< (字典序) */
    {
        STL_string x = stl_string_from_c(ar, "apple");
        STL_string same = stl_string_from_c(ar, "apple");
        STL_string y = stl_string_from_c(ar, "banana");
        STL_string pfx = stl_string_from_c(ar, "app");   /* x 的前缀 */

        CHECK(x == same);                    /* operator== */
        CHECK(!(x == y));                    /* 不同 */
        CHECK(x < y);                        /* operator<: 字典序 */
        CHECK(!(y < x));
        CHECK(pfx < x);                      /* 前缀更短 → 更小 */
        CHECK(x == x);                       /* 自反 */
    }

    stl_arena_destroy(ar);
    return 0;
}