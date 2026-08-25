/* t032c_model_edge.c — model 泛型边界组合收敛 (TODO P1)
 *
 * 多类型参数 + 嵌套实例化 + 常量参数组合用例, 确保实例化点展开与标准 C
 * 编译在所有边界下一致 (tcc 本地 == 脱糖产物 clang):
 *   A. 多类型参数 (T,U / T,U,V), 含多类型+嵌套实参组合;
 *   B. 嵌套实例化 2/3 层, 嵌套+多类型混合;
 *   C. 泛型递归: struct 关键字自引用 + 裸名自引用 + 双自引用字段,
 *      不同类型实例独立 (尺寸/布局不串);
 *   D. 常量参数: 多常量×类型, 常量实参传给嵌套实例, 归一化缓存 (2+2==4);
 *   E. 函数泛型: 实参/返回类型为嵌套实例;
 *   F. 缓存一致性: 同参多次实例化同内部类型 (sizeof 一致)。
 * 退出码 0 = 通过。构建: bin/tcc.exe tests/t032c_model_edge.c -o t032c_model_edge.exe
 */
#include <stdio.h>

/* A. 多类型参数 */
model struct Pair2(T, U) { T a; U b; };
model struct Tri(T, U, V) { T a; U b; V c; };

/* B. 嵌套实例化基元 */
model struct Box(T) { T v; };
model struct Array(T) { T *data; int len; };
model struct Wrap(T) { T inner; };

/* C. 泛型递归: struct 关键字自引用 / 裸名自引用 / 双自引用字段 */
model struct Node(T) { T v; struct Node(T) *next; };
model struct L(T) { T v; L(T) *next; };
model struct Tree(T) { T v; struct Tree(T) *l, *r; };

/* D. 常量参数: 多常量×类型; 常量实参传给嵌套实例 (Grid→Mat) */
model struct Mat(T, int R, int C) { T data[R * C]; int rows; };
model struct Grid(int N, T) { Mat(T, N, 2) row; };
model (int N, T) T vecsum(T a[N]) { T s = 0; for (int i = 0; i < N; i++) s += a[i]; return s; }

/* E. 函数泛型: 实参/返回类型为嵌套实例 */
model struct Idx(T) { T i; };
model (T) T idxget(Idx(T) *p) { return p->i; }

int main(void) {
    int fail = 0;

    /* A: 多类型参数 */
    {
        Pair2(int, double) p = { 7, 2.5 };
        if (p.a != 7 || p.b != 2.5) { puts("FAIL A1"); fail = 1; }
        Pair2(Pair2(int, int), double) pp;      /* 多类型 + 嵌套实参 */
        pp.a.a = 3; pp.a.b = 4; pp.b = 1.5;
        if (pp.a.a != 3 || pp.a.b != 4 || pp.b != 1.5) { puts("FAIL A2"); fail = 1; }
        Tri(int, float, double) t = { 1, 2.f, 3.0 };
        if (t.a != 1 || t.b != 2.f || t.c != 3.0) { puts("FAIL A3"); fail = 1; }
    }

    /* B: 嵌套实例化 2/3 层 */
    {
        Box(int) bx = { 42 };
        Array(Box(int)) arr;
        arr.data = &bx; arr.len = 1;
        if (arr.data[0].v != 42) { puts("FAIL B1"); fail = 1; }
        Wrap(Array(Box(int))) w;                /* 3 层 */
        w.inner.data = &bx; w.inner.len = 1;
        if (w.inner.data[0].v != 42) { puts("FAIL B2 (3-level)"); fail = 1; }
        Pair2(Box(int), Array(double)) mix;     /* 嵌套 + 多类型 */
        Box(int) b2 = { 9 };
        double dv = 3.5;
        mix.a = b2; mix.b.data = &dv; mix.b.len = 1;
        if (mix.a.v != 9 || mix.b.data[0] != 3.5) { puts("FAIL B3"); fail = 1; }
    }

    /* C: 泛型递归 */
    {
        Node(int) n1 = { 1, 0 }, n2 = { 2, &n1 };
        if (n2.next->v != 1) { puts("FAIL C1 (struct self-ref)"); fail = 1; }
        L(double) l1 = { 1.5, 0 }, l2 = { 2.5, &l1 };
        if (l2.next->v != 1.5) { puts("FAIL C2 (bare self-ref)"); fail = 1; }
        Tree(int) tr = { 7, 0, 0 }, t2 = { 8, &tr, 0 };
        if (t2.l->v != 7) { puts("FAIL C3 (two self-refs)"); fail = 1; }
        if (sizeof(((Node(int) *)0)->v) != 4
            || sizeof(((Node(double) *)0)->v) != 8) { puts("FAIL C4 (distinct)"); fail = 1; }
    }

    /* D: 常量参数组合 */
    {
        Mat(float, 4, 3) m;
        if (sizeof(m.data) != 12 * sizeof(float)) { puts("FAIL D1"); fail = 1; }
        Grid(3, int) g;                         /* 常量实参传嵌套实例 */
        if (sizeof(g.row) != sizeof(Mat(int, 3, 2))) { puts("FAIL D2 (const nest)"); fail = 1; }
        float arr[4] = { 1.f, 2.f, 3.f, 4.f };
        if (vecsum(4, float)(arr) != 10.f) { puts("FAIL D3"); fail = 1; }
        if (vecsum(2 + 2, float)(arr) != 10.f) { puts("FAIL D4 (normalize)"); fail = 1; }
    }

    /* E: 函数泛型 + 嵌套实例类型 */
    {
        Idx(int) ix = { 5 };
        if (idxget(int)(&ix) != 5) { puts("FAIL E1"); fail = 1; }
        Idx(Box(int)) ib;
        ib.i.v = 11;
        Box(int) r = idxget(Box(int))(&ib);     /* 返回类型是嵌套实例 */
        if (r.v != 11) { puts("FAIL E2 (nested ret)"); fail = 1; }
    }

    /* F: 缓存一致性 */
    {
        if (sizeof(Box(int)) != sizeof(Box(int))) { puts("FAIL F1"); fail = 1; }
        Pair2(int, int) *q = &(Pair2(int, int)){ 3, 4 };   /* 复合字面量 + 实例 */
        if (q->a != 3 || q->b != 4) { puts("FAIL F2"); fail = 1; }
    }

    if (fail) { puts("FAIL: t032c_model_edge"); return 1; }
    puts("PASS: t032c_model_edge");
    return 0;
}
