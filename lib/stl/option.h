/* tcc-stl option.h - model STL_Option(T)
 *
 * 可选值(POD 值语义)。类似 Rust Option<T>: 要么 Some(T), 要么 None。
 * 存储: 内嵌 tag (is_some) + T val (None 时 val 未初始化, 不参与运算)。
 *
 * 调用风格(与其余 STL 一致): 显式实例化 `stl_opt_unwrap(int)(&o)`。
 * 覆盖操作:
 *   stl_opt_some(T) / stl_opt_none(T)          构造
 *   stl_opt_is_some / stl_opt_is_none          谓词
 *   stl_opt_unwrap / stl_opt_unwrap_or /unwrap_or_else  取值
 *   stl_opt_map / stl_opt_and_then              组合子 (改类型 T→U)
 *
 * 组合记号: 保持"自包含"——map/and_then 就地构造结果, 不回调同参另机构造。
 */
#ifndef STL_OPTION_H
#define STL_OPTION_H

#include "allocator.h"

model struct STL_Option(T) {
    int is_some;
    T val;
};

/* --- 构造 --- */

model (T) STL_Option(T) stl_opt_some(T v) {
    STL_Option(T) o; o.is_some = 1; o.val = v; return o;
}
/* None: 无值。返回的 Option.is_some=0, val 留空。 */
model (T) STL_Option(T) stl_opt_none(void) {
    STL_Option(T) o; o.is_some = 0; return o;
}

/* --- 谓词 --- */

model (T) int stl_opt_is_some(const STL_Option(T) *o) { return o->is_some; }
model (T) int stl_opt_is_none(const STL_Option(T) *o) { return !o->is_some; }

/* --- 取值 --- */

/* unwrap: 断言 Some; None 时 STL_ASSERT 失败(文件:行)。 */
model (T) T stl_opt_unwrap(const STL_Option(T) *o) {
    STL_ASSERT(o && o->is_some);
    return o->val;
}
/* unwrap_or: None 时给默认值 (dflt 常求值)。 */
model (T) T stl_opt_unwrap_or(const STL_Option(T) *o, T dflt) {
    return o->is_some ? o->val : dflt;
}
/* unwrap_or_else: None 时惰性求值 f() (f 不自包含, 不回调本名)。 */
model (T) T stl_opt_unwrap_or_else(const STL_Option(T) *o, T (*f)(void)) {
    return o->is_some ? o->val : f();
}

/* --- 组合子 (改类型 T→U, 就地构造结果) --- */

/* map: 仅 Some 时应用 f(T)→U, 结果 Some(U); None 保持 None。 */
model (T, U) STL_Option(U) stl_opt_map(const STL_Option(T) *o, U (*f)(T)) {
    STL_Option(U) r;
    r.is_some = o->is_some;
    if (o->is_some) r.val = f(o->val);
    return r;
}
/* and_then: 仅 Some 时调用 f(T)→STL_Option(U), 平展开为 STL_Option(U); None 保持 None。 */
model (T, U) STL_Option(U) stl_opt_and_then(const STL_Option(T) *o,
                                            STL_Option(U) (*f)(T)) {
    STL_Option(U) r;
    if (!o->is_some) { r.is_some = 0; return r; }
    return f(o->val);
}

#endif /* STL_OPTION_H */