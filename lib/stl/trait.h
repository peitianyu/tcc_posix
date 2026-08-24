/* tcc-stl trait.h - 类型契约 (比较 / 判等), operator 驱动
 *
 * 约束: 本 tcc 的 `operator` 重载是**编译期静态分派**, 绑定具体类型
 * (`operator_lt(Vec3,Vec3)`), 无法基于 `model` 泛型定义(operator 不支持类型参数)。
 * 故 M0 泛型容器/算法对"元素比较"采取:
 *   - 内置标量(int/double/指针…): 用原生 `<` / `==`;
 *   - 用户自定义值类型: 为具体类型手写 `operator_lt` / `operator_eq`, 算法实例化
 *     重放时 `a < b` 自动分发到该 operator(谓词痛免费显式传比较器)。
 *
 * 本头只定义契约说明与显式比较回调类型(供 `slt_sort(..., comp)` 覆盖默认), 无实体约束。
 */
#ifndef SLT_TRAIT_H
#define SLT_TRAIT_H

/* 显式比较回调(可选覆盖默认谓词): 返回 <0 / 0 / >0 */
typedef int (*slt_less_fn)(const void *a, const void *b);

#endif /* SLT_TRAIT_H */