/* simd.h - SIMD 向量类型 (受控路径 lib/, M0 迁移; 见 docs/simd-standard.md)
 *
 *  M0 双模式版本: 保留 tcc 侧 struct v4f + clang 侧 __m128 双轨, 保证 tcc -run
 *  与脱糖产物各自可编译。M1(M2/M3) 单模式化后移除 __TCC_DESUGAR__/__GNUC__ 分支,
 *  统一为 __m128 家族 (见 docs/simd-standard.md §3.5)。
 *
 *  TCC 侧 (默认): v4f/v2d/v4i/v8h/v16b 为 16 字节对齐 struct, 经 x86_64-simd.c
 *    simd_vtype 按名解析, _mm_* 内建与 v4f+v4f 原生运算符发打包 SSE; v.x 字段可用.
 *  gcc/clang 侧: 映射 intrinsics 的 __m128/__m128d/__m128i, a+b 透传后原生编成
 *    addps/addpd/paddd; __m128 无 .x (标准交集局限).
 */
#ifndef TCC_POSIX_SIMD_H
#define TCC_POSIX_SIMD_H

/* __TCC_DESUGAR__: --emit-c 脱糖 (只出产物, 不运行) 时走 clang 侧 __m128 家族,
 * 使产物是标准 C + immintrin.h 原生 intrinsic; TCC -run 走下方 struct 分支. */
#if defined(__TCC_DESUGAR__) || (defined(__GNUC__) && !defined(__TINYC__))
#include <immintrin.h>
typedef __m128  v4f;
typedef __m128d v2d;
typedef __m128i v4i;
typedef __m128i v8h;
typedef __m128i v16b;
#else
typedef struct { float  x, y, z, w; } v4f  __attribute__((aligned(16)));
typedef struct { double x, y;        } v2d  __attribute__((aligned(16)));
typedef struct { int    x, y, z, w;  } v4i  __attribute__((aligned(16)));
typedef struct { short  x[8];        } v8h  __attribute__((aligned(16)));
typedef struct { signed char x[16];  } v16b __attribute__((aligned(16)));
#endif

#endif /* TCC_POSIX_SIMD_H */