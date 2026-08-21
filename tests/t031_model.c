/* 测试: model 泛型 struct/union */
#include <stdio.h>
model struct Array(T) { T *data; int len; };
model union Val(T) { T v; int tag; };

int main(void) {
    /* 1. 实例化 + 字段访问 */
    float buf[3] = { 1.5f, 2.5f, 3.5f };
    Array(float) a = { buf, 3 };
    if (a.len != 3 || a.data[1] != 2.5f) return 1;
    /* 2. 缓存复用: 同参类型一致 (sizeof 相同) */
    Array(float) b;
    if (sizeof a != sizeof b) return 2;
    /* 3. 多类型参数 / 不同实例互不影响 (T 直接作成员, size 随 T 变) */
    model struct Box(T) { T v; };
    if (sizeof(Box(double)) == sizeof(Box(int))) return 3;  /* 8 vs 4 */
    /* 4. union 实例化 */
    Val(int) u;
    u.v = 42;
    if (u.v != 42) return 4;
    /* 5. typedef 复用 */
    typedef Array(double) DArr;
    DArr d;
    if (sizeof d != sizeof(Array(double))) return 5;
    printf("model ok\n");
    return 0;
}
