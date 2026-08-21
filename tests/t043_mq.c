/* 测试: R10a mq (消息队列) syscall 补全.
 * musl mq 库层 (src/mq/mq_*.c) 走 SYS_mq_open(240)..SYS_mq_getsetattr(245),
 * PSX 接口层静态槽表实现. 覆盖:
 *  - mq_open(O_CREAT|O_RDWR) -> 非负 mqd
 *  - mq_getattr 读回 maxmsg/msgsize/curmsgs
 *  - mq_send / mq_receive 基础收发 + 内容一致
 *  - 优先级排序: 先发低优先级, 后发高优先级, 接收应得高优先级先
 *  - mq_setattr(O_NONBLOCK): 空队列 receive -> -1 errno=EAGAIN
 *  - mq_open(无 O_CREAT) 不存在 -> -1 errno=ENOENT
 *  - mq_open(O_CREAT|O_EXCL) 已存在 -> -1 errno=EEXIST
 *  - mq_unlink 后 mq_open 无 O_CREAT -> ENOENT
 *  - mq_close 释放 fd
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (errno=%d)\n", msg, errno); fail++; } \
    else printf("  ok: %s\n", msg); \
} while (0)

int main(void)
{
    mqd_t mq;
    struct mq_attr attr, attr2;
    char buf[64];
    unsigned prio;
    ssize_t n;
    int r;

    /* 1. 创建: O_CREAT|O_RDWR */
    attr.mq_flags = 0;
    attr.mq_maxmsg = 4;
    attr.mq_msgsize = 64;
    errno = 0;
    mq = mq_open("/t043q", O_CREAT | O_RDWR, 0666, &attr);
    CHECK(mq >= 0, "mq_open(O_CREAT)");

    /* 2. getattr */
    errno = 0;
    r = mq_getattr(mq, &attr2);
    CHECK(r == 0, "mq_getattr");
    if (r == 0) {
        printf("  attr: flags=%ld maxmsg=%ld msgsize=%ld curmsgs=%ld\n",
            attr2.mq_flags, attr2.mq_maxmsg, attr2.mq_msgsize, attr2.mq_curmsgs);
        CHECK(attr2.mq_curmsgs == 0, "curmsgs==0 初始");
    }

    /* 3. 基础收发 */
    errno = 0;
    r = mq_send(mq, "hello", 5, 0);
    CHECK(r == 0, "mq_send");
    memset(buf, 0, sizeof(buf));
    prio = 999;
    errno = 0;
    n = mq_receive(mq, buf, sizeof(buf), &prio);
    CHECK(n == 5, "mq_receive len");
    CHECK(memcmp(buf, "hello", 5) == 0, "mq_receive content");
    CHECK(prio == 0, "mq_receive prio");

    /* 4. 优先级: 先发低后发高, 接收应得高优先级先 */
    mq_send(mq, "low", 3, 1);
    mq_send(mq, "high", 4, 10);
    mq_send(mq, "mid", 3, 5);
    memset(buf, 0, sizeof(buf));
    n = mq_receive(mq, buf, sizeof(buf), &prio);
    CHECK(n == 4 && memcmp(buf, "high", 4) == 0 && prio == 10, "prio high first");
    memset(buf, 0, sizeof(buf));
    n = mq_receive(mq, buf, sizeof(buf), &prio);
    CHECK(n == 3 && memcmp(buf, "mid", 3) == 0 && prio == 5, "prio mid second");
    memset(buf, 0, sizeof(buf));
    n = mq_receive(mq, buf, sizeof(buf), &prio);
    CHECK(n == 3 && memcmp(buf, "low", 3) == 0 && prio == 1, "prio low last");

    /* 5. setattr O_NONBLOCK: 空队列 receive -> EAGAIN */
    attr.mq_flags = O_NONBLOCK;
    errno = 0;
    r = mq_setattr(mq, &attr, 0);
    CHECK(r == 0, "mq_setattr(NONBLOCK)");
    errno = 0;
    n = mq_receive(mq, buf, sizeof(buf), &prio);
    CHECK(n == -1 && errno == EAGAIN, "empty NONBLOCK receive -> EAGAIN");

    /* 6. 阻塞模式收到刚发的消息 */
    attr.mq_flags = 0;
    mq_setattr(mq, &attr, 0);
    mq_send(mq, "after", 5, 0);
    errno = 0;
    n = mq_receive(mq, buf, sizeof(buf), &prio);
    CHECK(n == 5 && memcmp(buf, "after", 5) == 0, "blocking receive after send");

    /* 7. O_EXCL 已存在 -> EEXIST */
    errno = 0;
    mq = mq_open("/t043q", O_CREAT | O_EXCL | O_RDWR, 0666, 0);
    CHECK(mq == -1 && errno == EEXIST, "mq_open(O_CREAT|O_EXCL) existing -> EEXIST");

    /* 8. 无 O_CREAT 打开已存在队列 */
    errno = 0;
    mq = mq_open("/t043q", O_RDWR, 0, 0);
    CHECK(mq >= 0, "mq_open(no O_CREAT) existing");
    if (mq >= 0) mq_close(mq);

    /* 9. 无 O_CREAT 打开不存在 -> ENOENT */
    errno = 0;
    mq = mq_open("/t043none", O_RDWR, 0, 0);
    CHECK(mq == -1 && errno == ENOENT, "mq_open(no O_CREAT) missing -> ENOENT");

    /* 10. mq_unlink 后无 O_CREAT -> ENOENT */
    errno = 0;
    r = mq_unlink("/t043q");
    CHECK(r == 0, "mq_unlink");
    errno = 0;
    mq = mq_open("/t043q", O_RDWR, 0, 0);
    CHECK(mq == -1 && errno == ENOENT, "after unlink -> ENOENT");

    /* 11. 清理: 打开并 close 剩余 fd (上面第 1 步的 mq 还在) */
    mq = mq_open("/t043q", O_CREAT | O_RDWR, 0666, 0);
    if (mq >= 0) { r = mq_close(mq); CHECK(r == 0, "mq_close"); }
    mq_unlink("/t043q");

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("mq ok\n");
    return 0;
}
