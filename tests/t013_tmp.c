/* 测试: /tmp 映射到用户临时目录 (tcc_posix: psxscl 兼容层) */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
int main(void) {
    const char *fn = "/tmp/tcc_tmp_map.txt";
    unlink(fn);
    /* 写 */
    int fd = open(fn, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) return 1;
    if (write(fd, "tmp mapping", 11) != 11) return 2;
    close(fd);
    /* 读回 */
    fd = open(fn, O_RDONLY);
    if (fd < 0) return 3;
    char buf[32];
    int n = read(fd, buf, sizeof buf);
    close(fd);
    buf[n] = 0;
    if (strcmp(buf, "tmp mapping")) return 4;
    /* stat */
    struct stat st;
    if (stat(fn, &st)) return 5;
    if (st.st_size != 11) return 6;
    if (!S_ISREG(st.st_mode)) return 7;
    /* 追加 */
    fd = open(fn, O_WRONLY | O_APPEND);
    if (fd < 0) return 8;
    write(fd, "!", 1);
    close(fd);
    fd = open(fn, O_RDONLY);
    n = read(fd, buf, sizeof buf);
    close(fd);
    buf[n] = 0;
    if (strcmp(buf, "tmp mapping!")) return 9;
    /* 删除 */
    if (unlink(fn)) return 10;
    if (!access(fn, F_OK)) return 11;
    return 0;
}
