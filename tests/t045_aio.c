/* 测试: R12 aio (异步 I/O) 库层可用性.
 * musl aio 是用户态线程池实现 (src/aio/aio.c), 非 syscall.
 * 每笔 aio 创建一个 detached 后台完成线程, 完成后自动退出 (释放线程栈)。
 * 本测试覆盖:
 *  - aio_read 基础: 提交/轮询完成/aio_return 内容一致
 *  - aio_write: 写入并读回
 *  - aio_suspend: 阻塞等待完成
 *  - 并发压力: 32 笔并发 aio_read 全部正确 (后台线程同时/相继退出)
 *  - 完成后主线程立即退出 (触发后台线程完整退出路径, R12 崩溃关注点)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <aio.h>

static int fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (errno=%d)\n", msg, errno); fail++; } \
    else printf("  ok: %s\n", msg); \
} while (0)

/* 轮询等待单笔 aio 完成 (上限 5s) */
static int aio_wait(const struct aiocb *cb)
{
    int i;
    for (i = 0; i < 500; i++) {
        int e = aio_error(cb);
        if (e != EINPROGRESS) return e;
        usleep(10000);
    }
    return EINPROGRESS; /* 超时 */
}

int main(void)
{
    char tmp[] = "t045_aio.tmp";
    int fd, r, i, e;
    char buf[64], wbuf[64];
    struct aiocb cb;
    const struct aiocb *cbs[4];

    fd = open(tmp, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { printf("FAIL: open (errno=%d)\n", errno); return 1; }

    /* 1. aio_read 基础 */
    if (write(fd, "hello-aio-world\n", 16) != 16) {
        printf("FAIL: seed write (errno=%d)\n", errno); return 1;
    }
    memset(&cb, 0, sizeof(cb));
    cb.aio_fildes = fd;
    cb.aio_buf = buf;
    cb.aio_nbytes = 16;
    cb.aio_offset = 0;
    errno = 0;
    r = aio_read(&cb);
    CHECK(r == 0, "aio_read submit");
    e = aio_wait(&cb);
    CHECK(e == 0, "aio_read completed");
    CHECK(aio_return(&cb) == 16, "aio_return == 16");
    CHECK(memcmp(buf, "hello-aio-world\n", 16) == 0, "aio_read content");

    /* 2. aio_write + 读回 */
    memset(&cb, 0, sizeof(cb));
    memcpy(wbuf, "wrote-by-aio\n", 13);
    cb.aio_fildes = fd;
    cb.aio_buf = wbuf;
    cb.aio_nbytes = 13;
    cb.aio_offset = 0;
    errno = 0;
    r = aio_write(&cb);
    CHECK(r == 0, "aio_write submit");
    e = aio_wait(&cb);
    CHECK(e == 0, "aio_write completed");
    CHECK(aio_return(&cb) == 13, "aio_write return == 13");

    memset(buf, 0, sizeof(buf));
    if (pread(fd, buf, 13, 0) != 13) {
        printf("FAIL: pread back (errno=%d)\n", errno); return 1;
    }
    CHECK(memcmp(buf, "wrote-by-aio\n", 13) == 0, "aio_write content");

    /* 3. aio_suspend 阻塞等待 */
    memset(&cb, 0, sizeof(cb));
    cb.aio_fildes = fd;
    cb.aio_buf = buf;
    cb.aio_nbytes = 13;
    cb.aio_offset = 0;
    r = aio_read(&cb);
    if (r == 0) {
        cbs[0] = &cb;
        errno = 0;
        r = aio_suspend(cbs, 1, 0);
        CHECK(r == 0, "aio_suspend returned");
        CHECK(aio_error(&cb) == 0, "aio_suspend op done");
    } else {
        printf("  skip: aio_suspend (submit errno=%d)\n", errno);
    }

    /* 4. 并发压力: 32 笔 aio_read (后台线程同时/相继退出) */
    {
        struct aiocb cbs32[32];
        char bufs[32][16];
        int done = 0;
        if (pwrite(fd, "0123456789abcdef", 16, 0) != 16) {
            printf("FAIL: seed2 write\n"); return 1;
        }
        for (i = 0; i < 32; i++) {
            memset(&cbs32[i], 0, sizeof(cbs32[i]));
            cbs32[i].aio_fildes = fd;
            cbs32[i].aio_buf = bufs[i];
            cbs32[i].aio_nbytes = 16;
            cbs32[i].aio_offset = 0;
            if (aio_read(&cbs32[i]) != 0) {
                printf("  skip: stress submit[%d] errno=%d\n", i, errno);
                break;
            }
        }
        for (i = 0; i < 32; i++) {
            if (aio_wait(&cbs32[i]) != 0) continue;
            if (aio_return(&cbs32[i]) == 16 &&
                memcmp(bufs[i], "0123456789abcdef", 16) == 0)
                done++;
        }
        CHECK(done == 32, "32 concurrent aio_read all correct");
    }

    close(fd);
    remove(tmp);

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("aio ok\n");
    return 0;
}
