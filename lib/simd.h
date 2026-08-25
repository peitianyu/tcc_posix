/* simd.h - SIMD 向量类型 (单模式: 标准 `__m128` 家族, 见 docs/simd-standard.md)
 *
 * M2 起单模式: TCC 与 gcc/clang 用同一 `__m128` 载体, 脱糖 --emit-c 对 SIMD 纯透传。
 * TCC (__TINYC__) 侧 `__m128` 是其内建向量类型(内核 VT_VECTOR, M1/M2), 不需
 * immintrin.h; gcc/clang 侧 include <immintrin.h> 提供标准 intrinsic。
 *
 * 标准交集局限: `__m128` 无 .x/.y/.z 字段访问(用下标 / ((float*)&v)[i] / _mm_cvtss_f32)。
 */
#ifndef TCC_POSIX_SIMD_H
#define TCC_POSIX_SIMD_H

/* __TCC_DESUGAR__: tcc --emit-c 脱糖时透传该 include 进产物 (tcc 不解析 immintrin.h,
 * 原生 gcc/clang 编译产物时由它提供 __m128/_mm_*). TCC -run 走内建 __m128 不需头. */
#if defined(__TCC_DESUGAR__) || (defined(__GNUC__) && !defined(__TINYC__))
#include <immintrin.h>
#endif

typedef __m128  v4f;
typedef __m128d v2d;
typedef __m128i v4i;
typedef __m128i v8h;
typedef __m128i v16b;

#endif /* TCC_POSIX_SIMD_H */