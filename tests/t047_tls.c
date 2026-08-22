/* 测试: __thread / thread_local (emutls) — 读写在多 TLS 对象间不冲突,
 * 初始化值(defval)物化、取址、数组索引均正确。 */
#include <stdio.h>

__thread int tlv = 42;          /* 带初始值 → defval 物化 */
__thread long tls_arr[4];       /* 无初始值 → 零初始化 */
__thread char tlv2;             /* 第二个独立 TLS 对象 */
__thread struct { int a; long b; } tls_st;   /* 结构体 TLS */

/* 跨函数使用同一个 TLS (验证多次 __emutls_get_address 返回同一地址) */
static int bump(void)
{
    tlv += 1;
    return tlv;
}

int main(void)
{
    int *p;
    if (tlv != 42) { printf("FAIL init tlv=%d\n", tlv); return 1; }

    tls_arr[0] = 7; tls_arr[3] = 9;
    if (tls_arr[0] + tls_arr[3] != 16) { printf("FAIL arr\n"); return 1; }

    tlv += 1;                    /* 复合赋值 */
    if (tlv != 43) { printf("FAIL += tlv=%d\n", tlv); return 1; }

    if (bump() != 44) { printf("FAIL cross-fn tlv=%d\n", tlv); return 1; }
    if (tlv != 44) { printf("FAIL persists tlv=%d\n", tlv); return 1; }

    p = &tlv;                    /* 取址 → 指向 per-thread 存储 */
    *p = 100;
    if (tlv != 100) { printf("FAIL via-ptr tlv=%d\n", tlv); return 1; }

    tlv2 = 'x';
    tls_st.a = 33; tls_st.b = 1L << 40;
    if (tlv2 != 'x' || tls_st.a != 33 || tls_st.b != (1L << 40)) {
        printf("FAIL struct/char collision\n"); return 1;
    }

    /* 各 TLS 对象互不别名 */
    if (p == (int *)&tls_arr[0]) { printf("FAIL alias\n"); return 1; }

    printf("tls ok tlv=%d arr=%ld char=%d struct=%ld\n",
           tlv, tls_arr[0] + tls_arr[3], tlv2,
           tls_st.b);
    return 0;
}