/* t080_stl_option: model STL_Option(T) — 可选值(类似 Rust Option<T>)
 *
 * 纯断言(无 stdio)。覆盖:
 *   1. some/none 构造 + is_some/is_none 谓词
 *   2. unwrap / unwrap_or / unwrap_or_else 取值(Some 有值, None 走默认/惰性)
 *   3. map / and_then 组合子(改类型 T→U, None 短路保 None)
 *   4. 多实例: int / double / STL_Pair(int,int) 复合元素, None 不读未初始化 val
 * 调用风格: 显式实例化 `stl_opt_*(T)(...)`。
 *
 * 闭环约束(见 docs/desugar.md "EOF 回放 typedef/函数两段"): 用户回调(定义于
 * main 前)不得再调用 model 构造器(构造器在 EOF 冲排、位于用户代码之后) →
 * 回调体内 inline 构造。类型实参用 model 类型(STL_Pair)而非用户 struct,
 * 避免 typedef 先于其定义。
 * 退出码 0 = 通过.
 */
#include "lib/stl/option.h"
#include "lib/stl/pair.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* unwrap_or_else 惰性回调 */
static int fallback_default(void) { return -1; }
/* map 回调 (纯函数, 不触碰 model 构造器) */
static double square_int(int x) { return (double)x * (double)x; }
static int pair_sum(STL_Pair(int,int) p) { return p.first + p.second; }
/* and_then 回调: int→Option(double); 内联构造(避免引用 EOF 冲排的构造器) */
static STL_Option(double) half_if_pos(int x) {
    STL_Option(double) r;
    r.is_some = (x >= 0);
    if (r.is_some) r.val = x * 0.5;
    return r;
}

int main(void) {
    /* 1. 构造 + 谓词 */
    STL_Option(int) so = stl_opt_some(int)(42);
    STL_Option(int) no = stl_opt_none(int)();
    CHECK(stl_opt_is_some(int)(&so));
    CHECK(!stl_opt_is_none(int)(&so));
    CHECK(stl_opt_is_none(int)(&no));
    CHECK(!stl_opt_is_some(int)(&no));

    /* 2. 取值 */
    CHECK(stl_opt_unwrap(int)(&so) == 42);
    CHECK(stl_opt_unwrap_or(int)(&no, 7) == 7);
    CHECK(stl_opt_unwrap_or(int)(&so, 7) == 42);
    CHECK(stl_opt_unwrap_or_else(int)(&no, fallback_default) == -1);
    CHECK(stl_opt_unwrap_or_else(int)(&so, fallback_default) == 42);

    /* 3. 组合子: Some → Some(map 结果); None → None(短路) */
    STL_Option(double) mo = stl_opt_map(int, double)(&so, square_int);
    STL_Option(double) mn = stl_opt_map(int, double)(&no, square_int);
    CHECK(stl_opt_is_some(double)(&mo));
    CHECK(stl_opt_unwrap(double)(&mo) == 42.0 * 42.0);
    CHECK(stl_opt_is_none(double)(&mn));

    STL_Option(double) ht = stl_opt_and_then(int, double)(&so, half_if_pos);
    STL_Option(double) hn = stl_opt_and_then(int, double)(&no, half_if_pos);
    CHECK(stl_opt_unwrap(double)(&ht) == 21.0);
    CHECK(stl_opt_is_none(double)(&hn));

    /* 4. 多实例 + 复合元素(STL_Pair) + None 不读未初始化 */
    STL_Option(double) sd = stl_opt_some(double)(1.5);
    CHECK(stl_opt_unwrap(double)(&sd) == 1.5);

    STL_Option(STL_Pair(int,int)) sp = stl_opt_some(STL_Pair(int,int))((STL_Pair(int,int)){3, 4});
    CHECK(stl_opt_is_some(STL_Pair(int,int))(&sp));
    CHECK(stl_opt_unwrap(STL_Pair(int,int))(&sp).first == 3);

    STL_Option(int) spm = stl_opt_map(STL_Pair(int,int), int)(&sp, pair_sum);
    CHECK(stl_opt_unwrap(int)(&spm) == 7);

    /* None 的 map 二次短路(不调用回调) */
    STL_Option(int) nonce = stl_opt_map(double, int)(&hn, (int (*)(double))0);
    CHECK(stl_opt_is_none(int)(&nonce));

    return 0;
}