/* 测试: model 泛型函数 (无 function 关键字, model (T) 返回类型 函数名) */
#include <stdio.h>
model (T) T max2(T a, T b) { return a > b ? a : b; }
model (T) void swap2(T *a, T *b) { T t = *a; *a = *b; *b = t; }

int main(void) {
    /* 1. 实例化调用 + 返回值 */
    if (max2(int)(3, 7) != 7) return 1;
    if (max2(double)(2.5, 1.5) != 2.5) return 2;
    /* 2. void 返回 + 函数体内 T 局部变量 */
    int x = 1, y = 2;
    swap2(int)(&x, &y);
    if (x != 2 || y != 1) return 3;
    /* 3. 嵌套 struct 实参 */
    model struct Pair(T) { T a, b; };
    model (T) T first2(Pair(T) *p) { return p->a; }
    Pair(int) p = { 9, 8 };
    if (first2(int)(&p) != 9) return 4;
    printf("model fn ok\n");
    return 0;
}
