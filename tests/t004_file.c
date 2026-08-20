/* 测试: 文件 IO (open/read/write/lseek/stat/rename/unlink) */
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
int main(void) {
    const char *fn = "t003_file.txt";
    unlink(fn);
    /* 写 */
    int fd = open(fn, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) return 1;
    if (write(fd, "hello world", 11) != 11) return 2;
    if (lseek(fd, 6, SEEK_SET) != 6) return 3;
    if (write(fd, "W", 1) != 1) return 4;   /* 覆盖: hellow world */
    if (lseek(fd, 0, SEEK_END) != 11) return 5;
    if (lseek(fd, 0, SEEK_CUR) != 11) return 6;
    close(fd);
    /* stat */
    struct stat st;
    if (stat(fn, &st)) return 7;
    if (st.st_size != 11) return 8;
    if (!S_ISREG(st.st_mode)) return 9;
    /* 读 */
    fd = open(fn, O_RDONLY);
    if (fd < 0) return 10;
    char buf[64];
    int n = read(fd, buf, sizeof buf);
    if (n != 11) return 11;
    buf[n] = 0;
    if (strcmp(buf, "hello World")) return 12;   /* 位置6被W覆盖 */
    /* pread 不影响偏移 */
    if (pread(fd, buf, 5, 0) != 5) return 13;
    if (lseek(fd, 0, SEEK_CUR) != 11) return 14;
    close(fd);
    /* rename */
    if (rename(fn, "t003_file2.txt")) return 15;
    /* access (0644: 所有者可读写) */
    if (access("t003_file2.txt", R_OK)) return 16;
    if (access("t003_file2.txt", W_OK)) return 17;
    /* unlink */
    if (unlink("t003_file2.txt")) return 18;
    if (!access("t003_file2.txt", F_OK)) return 19;
    /* 错误处理 */
    errno = 0;
    if (open("t003_nonexistent.txt", O_RDONLY) >= 0) return 20;
    if (errno != ENOENT) return 21;
    /* 追加模式 */
    fd = open(fn, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) return 22;
    write(fd, "X", 1);
    close(fd);
    fd = open(fn, O_RDONLY);
    n = read(fd, buf, sizeof buf);
    close(fd);
    if (n != 1 || buf[0] != 'X') return 23;
    unlink(fn);
    return 0;
}
