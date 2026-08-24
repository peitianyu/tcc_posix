/* simd.h - 双模式 SIMD 向量类型
 *
 *  TCC 侧 (默认): v4f/v2d/v4i/v8h/v16b 为 16 字节对齐 struct, 编译器
 *    (x86_64-simd.c) 经 simd_vtype 按名解析出这些 struct 符号, 通过
 *    _mm_* 内建与 v4f+v4f 原生运算符发射打包 SSE 指令. 普通字段访问
 *    (如 v.x) 天然可用.
 *
 *  gcc/clang 侧 (--emit-c 脱糖产物, 交原生编译): 映射到 intrinsics 的
 *    __m128/__m128d/__m128i, 使 a+b 脱糖透传后由 gcc/clang 原生编成
 *    addps/addpd/paddd 等. 注意 __m128 无 .x 等字段访问(标准内建交集局限).
 */
#ifndef TCC_POSIX_SIMD_H
#define TCC_POSIX_SIMD_H

#if defined(__GNUC__) && !defined(__TINYC__)
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