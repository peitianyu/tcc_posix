/* 测试: model 泛型编译期整型常量参数 (int R 声明, 归一化求值缓存) */
#include <stdio.h>

/* struct 泛型: 类型参数 T + 常量参数 int R/C (类似 std::array<T,N>) */
model struct Mat(T, int R, int C) { T data[R * C]; int rows; };
model struct Vec(int N) { int sum[N]; };

/* function 泛型: 常量参数 + 类型参数混用 */
model (int N, T) T vecsum(T a[N]) { T s = 0; for (int i = 0; i < N; i++) s += a[i]; return s; }
model (int N) int rangemax(void) { int m = 0; for (int i = 0; i < N; i++) if (i > m) m = i; return m; }

int main(void) {
    /* 1. 常量尺寸数组成员 (R*C 布局期求值) */
    Mat(float, 4, 3) m;
    if (sizeof(m.data) != 12 * sizeof(float)) return 1;   /* 4*3*4 */
    if (sizeof(Mat(float, 4, 3)) != 12 * sizeof(float) + sizeof(int)) return 2;
    m.data[5] = 1.5f;
    if (m.data[5] != 1.5f) return 3;

    /* 2. 归一化求值: 2+2 与 4 共享同一内部类型 (sizeof 相等即同缓存) */
    Mat(float, 2 + 2, 3) m2;
    if (sizeof m2 != sizeof m) return 4;

    /* 3. 常量参数驱动 data 长度 (类型随 R/C 变, 不同实例 size 不同) */
    Vec(8) v8;
    Vec(16) v16;
    if (sizeof v8 >= sizeof v16) return 5;

    /* 4. function 泛型: 常量 N 决定数组循环长度 */
    float arr[4] __attribute__((aligned(16))) = { 1.5f, 2.5f, 3.5f, 4.5f };
    if (!(vecsum(4, float)(arr) == 12.0f)) return 6;
    /* 归一化: vecsum(2+2, float) 与 vecsum(4, float) 是同一函数 */
    if (!(vecsum(2 + 2, float)(arr) == 12.0f)) return 7;
    /* 纯常量参数 (无类型参数) */
    if (rangemax(5)() != 4) return 8;

    printf("model const ok\n");
    return 0;
}