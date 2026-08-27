/* tcc-stl result.h - model STL_Result(R,E)
 *
 * 结果(POD 值语义)。类似 Rust Result<R,E>: 要么 Ok(R), 要么 Err(E)。
 * 存储: 内嵌 tag (is_ok) + union{ R ok; E err; } (未命中的另一分支未初始化,
 * 不参与运算; union 大小 = max(sizeof R, sizeof E), 值语义按需携带其一)。
 *
 * 调用风格(与其余 STL 一致): 显式实例化 `stl_res_unwrap(int,const char*)(&r)`。
 * 覆盖操作:
 *   stl_res_ok / stl_res_err        构造
 *   stl_res_is_ok / stl_res_is_err  谓词
 *   stl_res_unwrap / unwrap_err /unwrap_or / expect  取值
 *   stl_res_map / map_err / and_then / or_else        组合子
 */
#ifndef STL_RESULT_H
#define STL_RESULT_H

#include "allocator.h"

model struct STL_Result(R,E) {
    int is_ok;
    union { R ok; E err; };
};

/* --- 构造 --- */

model (R,E) STL_Result(R,E) stl_res_ok(R v) {
    STL_Result(R,E) r; r.is_ok = 1; r.ok = v; return r;
}
model (R,E) STL_Result(R,E) stl_res_err(E e) {
    STL_Result(R,E) r; r.is_ok = 0; r.err = e; return r;
}

/* --- 谓词 --- */

model (R,E) int stl_res_is_ok(const STL_Result(R,E) *r) { return r->is_ok; }
model (R,E) int stl_res_is_err(const STL_Result(R,E) *r) { return !r->is_ok; }

/* --- 取值 --- */

/* unwrap: Ok 返回 ok; Err 时 STL_ASSERT 失败(文件:行)。 */
model (R,E) R stl_res_unwrap(const STL_Result(R,E) *r) {
    STL_ASSERT(r && r->is_ok);
    return r->ok;
}
/* unwrap_err: 仅 Err 时返回 err; Ok 时断言失败。 */
model (R,E) E stl_res_unwrap_err(const STL_Result(R,E) *r) {
    STL_ASSERT(r && !r->is_ok);
    return r->err;
}
/* unwrap_or: Err 时给默认值 (常求值)。 */
model (R,E) R stl_res_unwrap_or(const STL_Result(R,E) *r, R dflt) {
    return r->is_ok ? r->ok : dflt;
}
/* expect: Ok 返回 ok; Err 时断言失败并带上 msg(文件:行)。 */
model (R,E) R stl_res_expect(const STL_Result(R,E) *r, const char *msg) {
    STL_ASSERT(r && r->is_ok && *msg != 0);
    return r->ok;
}

/* --- 组合子 (改类型 R→U 或 E→F, 就地构造结果) --- */

/* map: Ok 时应用 f(R)→U; Err 保持 Err。 */
model (R,E,U) STL_Result(U,E) stl_res_map(const STL_Result(R,E) *r, U (*f)(R)) {
    STL_Result(U,E) x;
    if (r->is_ok) { x.is_ok = 1; x.ok = f(r->ok); }
    else          { x.is_ok = 0; x.err = r->err; }
    return x;
}
/* map_err: Err 时应用 f(E)→F; Ok 保持 Ok。 */
model (R,E,F) STL_Result(R,F) stl_res_map_err(const STL_Result(R,E) *r, F (*f)(E)) {
    STL_Result(R,F) x;
    if (r->is_ok) { x.is_ok = 1; x.ok = r->ok; }
    else          { x.is_ok = 0; x.err = f(r->err); }
    return x;
}
/* and_then: Ok 时调用 f(R)→STL_Result(U,E), 平展开; Err 保持 Err。 */
model (R,E,U) STL_Result(U,E) stl_res_and_then(const STL_Result(R,E) *r,
                                               STL_Result(U,E) (*f)(R)) {
    STL_Result(U,E) x;
    if (r->is_ok) return f(r->ok);
    x.is_ok = 0; x.err = r->err; return x;
}
/* or_else: Err 时调用 f(E)→STL_Result(R,F), 平展开; Ok 保持 Ok。 */
model (R,E,F) STL_Result(R,F) stl_res_or_else(const STL_Result(R,E) *r,
                                              STL_Result(R,F) (*f)(E)) {
    STL_Result(R,F) x;
    if (r->is_ok) { x.is_ok = 1; x.ok = r->ok; return x; }
    return f(r->err);
}

#endif /* STL_RESULT_H */