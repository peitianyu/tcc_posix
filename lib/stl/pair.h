/* tcc-stl pair.h - model STL_Pair(A,B)
 *
 * 二元对: 直接的聚合(POD 值语义)。用聚合初始化构造:
 *   STL_Pair(int,float) p = { 1, 2.5f };
 *   p.first / p.second
 *
 * 说明: 不提供基于 model 泛型的 operator(本 tcc 无法给泛型类型定义 operator),
 * 元素级比较由 Pair 所包裹的具体类型各自的 operator 承担(M0c 算法层展开 first/second)。
 */
#ifndef STL_PAIR_H
#define STL_PAIR_H

model struct STL_Pair(A,B) { A first; B second; };

#endif /* STL_PAIR_H */