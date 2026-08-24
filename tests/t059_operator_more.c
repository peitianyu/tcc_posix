/* t059_operator_more.c — 运算符重载补全: 比较 / 一元 / 自增自减 / 复合赋值
 *
 * 覆盖新增运算符类别 (值语义静态分派):
 *   a == b, a != b, a < b, a <= b, a > b, a >= b   → operator== / != / < / <= / > / >= (返回 int)
 *   !a, ~a                                          → operator! / operator~
 *   ++a / a++ / --a / a--                          → operator++ / operator-- (值语义, 增后新struct)
 *   a += b, a -= b, a *= b, a /= b, a %= b         → operator+ - * / % 后存回
 * 说明: C 无重载, operator- 单名只能取其一二元取负之一 (Vec3 用作二元减法).
 * 退出码 0 = 通过.
 */
#include <stdio.h>

struct Vec3 { int x, y, z; };

static struct Vec3 operator+ (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x+b.x, a.y+b.y, a.z+b.z }; return r;
}
static struct Vec3 operator- (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x-b.x, a.y-b.y, a.z-b.z }; return r;
}
static struct Vec3 operator* (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x*b.x, a.y*b.y, a.z*b.z }; return r;
}
static struct Vec3 operator/ (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x/b.x, a.y/b.y, a.z/b.z }; return r;
}
static struct Vec3 operator% (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x%b.x, a.y%b.y, a.z%b.z }; return r;
}
static struct Vec3 operator! (struct Vec3 a) { struct Vec3 r = { !a.x, !a.y, !a.z }; return r; }
static struct Vec3 operator~ (struct Vec3 a) { struct Vec3 r = { ~a.x, ~a.y, ~a.z }; return r; }
static struct Vec3 operator++ (struct Vec3 a) { a.x++; a.y++; a.z++; return a; }
static struct Vec3 operator-- (struct Vec3 a) { a.x--; a.y--; a.z--; return a; }

static int operator==(struct Vec3 a, struct Vec3 b) { return a.x==b.x&&a.y==b.y&&a.z==b.z; }
static int operator!=(struct Vec3 a, struct Vec3 b) { return a.x!=b.x||a.y!=b.y||a.z!=b.z; }
static int operator< (struct Vec3 a, struct Vec3 b) { return a.x<b.x&&a.y<b.y&&a.z<b.z; }
static int operator<=(struct Vec3 a, struct Vec3 b) { return a.x<=b.x&&a.y<=b.y&&a.z<=b.z; }
static int operator> (struct Vec3 a, struct Vec3 b) { return a.x>b.x&&a.y>b.y&&a.z>b.z; }
static int operator>=(struct Vec3 a, struct Vec3 b) { return a.x>=b.x&&a.y>=b.y&&a.z>=b.z; }

static int is3(struct Vec3 v, int x, int y, int z) { return v.x==x&&v.y==y&&v.z==z; }

int main(void) {
    struct Vec3 a = { 1, 2, 3 };
    struct Vec3 b = { 4, 5, 6 };
    struct Vec3 e = { 1, 2, 3 };   /* 与 a 相等 */
    struct Vec3 s;

    /* ---- 比较 (返回 int) ---- */
    if (!(a == e))                    { puts("FAIL: a==e");       return 1; }
    if (a == b)                       { puts("FAIL: a==b");       return 1; }
    if (!(a != b))                    { puts("FAIL: a!=b");       return 1; }
    if (a != e)                       { puts("FAIL: a!=e");       return 1; }
    if (!(a < b))                     { puts("FAIL: a<b");        return 1; }
    if (b < a)                        { puts("FAIL: b<a");        return 1; }
    if (!(a <= e))                    { puts("FAIL: a<=e");       return 1; }
    if (!(a <= b))                    { puts("FAIL: a<=b");       return 1; }
    if (!(b > a))                     { puts("FAIL: b>a");        return 1; }
    if (a > b)                        { puts("FAIL: a>b");        return 1; }
    if (!(b >= a))                    { puts("FAIL: b>=a");       return 1; }
    if (!(e >= a))                    { puts("FAIL: e>=a");       return 1; }

    /* ---- 一元 ! / ~ ---- */
    s = !a;                           /* {!1,!2,!3}={0,0,0} */
    if (!is3(s, 0, 0, 0))             { puts("FAIL: operator!");  return 2; }
    s = ~a;                           /* {~1,~2,~3}={-2,-3,-4} */
    if (!is3(s, -2, -3, -4))          { puts("FAIL: operator~");  return 3; }

    /* ---- 前缀 ++ / -- ---- */
    s = a;                            /* {1,2,3} */
    ++s;                              /* {2,3,4} */
    if (!is3(s, 2, 3, 4))             { puts("FAIL: prefix ++");  return 4; }
    --s;                              /* {1,2,3} */
    if (!is3(s, 1, 2, 3))             { puts("FAIL: prefix --");  return 5; }

    /* 前缀结果作为表达式值 (应得到增后新值) */
    s = a;                            /* {1,2,3} */
    s = ++s;                          /* s 得到 {2,3,4} */
    if (!is3(s, 2, 3, 4))             { puts("FAIL: prefix ++ used as expr"); return 6; }

    /* ---- 后缀 -- / ++ (结果=旧值) ---- */
    s = a;                            /* {1,2,3} */
    s = s++;                          /* 旧值 {1,2,3} 赋给 s, s 此时为 {2,3,4} */
    if (!is3(s, 2, 3, 4))             { puts("FAIL: postfix ++ stored"); return 7; }
    s = a;
    s++;                              /* s={2,3,4} */
    if (!is3(s, 2, 3, 4))             { puts("FAIL: postfix ++ stmt");  return 8; }

    /* ---- 复合赋值 += -= *= /= %= ---- */
    s = a;   s += b;                  /* {5,7,9}  */
    if (!is3(s, 5, 7, 9))             { puts("FAIL: s+=b");       return 9;  }
    s = b;   s -= a;                  /* {3,3,3}  */
    if (!is3(s, 3, 3, 3))             { puts("FAIL: s-=a");       return 10; }
    s = a;   s *= b;                  /* {4,10,18} */
    if (!is3(s, 4, 10, 18))           { puts("FAIL: s*=b");       return 11; }
    s = b;   s /= a;                  /* {4,2,2}  */
    if (!is3(s, 4, 2, 2))             { puts("FAIL: s/=a");       return 12; }
    s = b;   s %= a;                  /* {0,1,0}  */
    if (!is3(s, 0, 1, 0))             { puts("FAIL: s%=a");       return 13; }

    /* 标量不误改 */
    int x = 1; x += 2;                /* 3  */
    if (x != 3)                       { puts("FAIL: scalar +=");  return 14; }
    if (!(5 > 3))                     { puts("FAIL: scalar >");    return 15; }

    puts("PASS: t059_operator_more");
    return 0;
}