/* t081_stl_result: model STL_Result(R,E) — 结果(类似 Rust Result<R,E>)
 *
 * 纯断言(无 stdio)。覆盖:
 *   1. ok/err 构造 + is_ok/is_err 谓词 + union 双分支互斥
 *   2. unwrap / unwrap_err / unwrap_or / expect 取值(Ok/Err 各自有值)
 *   3. map / map_err / and_then / or_else 组合子(改类型, Ok/Err 老实短路)
 *   4. 多实例: R=int E=const char*; R=double E=int; union 大小按大者
 * 调用风格: 显式实例化 `stl_res_*(R,E)(...)`。
 *
 * 闭环约束(见 docs/desugar.md "EOF 回放 typedef/函数两段"): 用户回调(定义于
 * main 前)不得再调用 model 构造器(EOF 冲排、位于用户代码之后) →
 * 回调体内 inline 构造 STL_Result。
 * 退出码 0 = 通过.
 */
#include "lib/stl/result.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* map 回调: 把 int 变 double */
static double scale2(int x) { return x * 2.0; }
/* map_err 回调: 把 const char* 变 int(错误码长度) */
static int err_len(const char *e) { int n = 0; while (e[n]) n++; return n; }
/* and_then 回调: int→Result(double,const char*); 内联构造 */
static STL_Result(double, const char *) checked_f(int x) {
    STL_Result(double, const char *) r;
    if (x >= 0) { r.is_ok = 1; r.ok = x * 1.5; }
    else        { r.is_ok = 0; r.err = "neg"; }
    return r;
}
/* or_else 回调: const char*→Result(int,int); 内联构造 */
static STL_Result(int, int) to_code(const char *e) {
    STL_Result(int, int) r;
    if (e[0] == 'n') { r.is_ok = 1; r.ok = -7; }
    else             { r.is_ok = 0; r.err = 99; }
    return r;
}

int main(void) {
    /* 1. 构造 + 谓词 */
    STL_Result(int, const char *) rok = stl_res_ok(int, const char *)(10);
    STL_Result(int, const char *) rerr = stl_res_err(int, const char *)("boom");
    CHECK(stl_res_is_ok(int, const char *)(&rok));
    CHECK(!stl_res_is_err(int, const char *)(&rok));
    CHECK(stl_res_is_err(int, const char *)(&rerr));
    CHECK(!stl_res_is_ok(int, const char *)(&rerr));

    /* 2. 取值: Ok 读 ok, Err 读 err, 各自互斥 */
    CHECK(stl_res_unwrap(int, const char *)(&rok) == 10);
    CHECK(stl_res_unwrap_err(int, const char *)(&rerr)[0] == 'b');
    CHECK(stl_res_unwrap_or(int, const char *)(&rerr, 5) == 5);
    CHECK(stl_res_unwrap_or(int, const char *)(&rok, 5) == 10);
    CHECK(stl_res_expect(int, const char *)(&rok, "should be ok") == 10);

    /* 3a. map: Ok → Ok(结果), Err 保持 Err */
    STL_Result(double, const char *) mrok = stl_res_map(int, const char *, double)(&rok, scale2);
    STL_Result(double, const char *) mrerr = stl_res_map(int, const char *, double)(&rerr, scale2);
    CHECK(stl_res_unwrap(double, const char *)(&mrok) == 20.0);
    CHECK(stl_res_is_err(double, const char *)(&mrerr));
    CHECK(stl_res_unwrap_err(double, const char *)(&mrerr)[0] == 'b');

    /* 3b. map_err: Err → Err(结果), Ok 保持 Ok */
    STL_Result(int, int) mok = stl_res_map_err(int, const char *, int)(&rok, err_len);
    STL_Result(int, int) merr = stl_res_map_err(int, const char *, int)(&rerr, err_len);
    CHECK(stl_res_unwrap(int, int)(&mok) == 10);
    CHECK(stl_res_unwrap_err(int, int)(&merr) == 4); /* "boom" 长 4 */

    /* 3c. and_then: Ok → 平铺 Result(U,E); Err 短路保持 Err */
    STL_Result(double, const char *) atk = stl_res_and_then(int, const char *, double)(&rok, checked_f);
    STL_Result(double, const char *) ate = stl_res_and_then(int, const char *, double)(&rerr, checked_f);
    CHECK(stl_res_unwrap(double, const char *)(&atk) == 15.0);
    CHECK(stl_res_is_err(double, const char *)(&ate));

    /* 3d. or_else: Err → 平铺 Result(R,F); Ok 短路保持 Ok */
    STL_Result(int, int) oe = stl_res_or_else(int, const char *, int)(&rerr, to_code);
    STL_Result(int, int) ok = stl_res_or_else(int, const char *, int)(&rok, to_code);
    /* rerr="boom"+b → to_code: b≠'n' → Err(99); rok=Ok10 → 短路保持 Ok(10) */
    CHECK(stl_res_is_err(int, int)(&oe));
    CHECK(stl_res_unwrap_err(int, int)(&oe) == 99);
    CHECK(stl_res_is_ok(int, int)(&ok));
    CHECK(stl_res_unwrap(int, int)(&ok) == 10);

    /* 4. 多实例 + union 大小按大者(不重排/不判等, 仅确认可独立携带) */
    STL_Result(double, int) rd = stl_res_ok(double, int)(2.5);
    CHECK(stl_res_unwrap(double, int)(&rd) == 2.5);
    STL_Result(double, int) rde = stl_res_err(double, int)(-3);
    CHECK(stl_res_unwrap_err(double, int)(&rde) == -3);

    return 0;
}