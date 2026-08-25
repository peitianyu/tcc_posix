/* libdemo_consumer.c — 消费端: 链接独立库并调用 API, 验证导出符号可解析 */
#include <stdio.h>

struct Pt { int x, y; };
struct Box_int;
struct Pt vec_add(struct Pt a, struct Pt b);
int vec_cmp(struct Pt a, struct Pt b);
struct Box_int *box_new_int(void);
int box_val(struct Box_int *b);
int pt_fields(void);
void deferred_work(int *out);

int main(void)
{
    struct Pt a = { 1, 2 }, b = { 3, 4 };
    struct Pt s = vec_add(a, b);
    int out = 0;

    if (s.x != 4 || s.y != 6) { puts("FAIL: vec_add"); return 1; }
    if (!vec_cmp(a, b))        { puts("FAIL: vec_cmp"); return 2; }
    if (box_val(box_new_int()) != 42) { puts("FAIL: box"); return 3; }
    if (pt_fields() != 2)      { puts("FAIL: pt_fields"); return 4; }
    deferred_work(&out);
    if (out != 7)              { puts("FAIL: deferred_work"); return 5; }
    puts("PASS: libdemo consumer");
    return 0;
}
