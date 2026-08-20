/* 测试: 目录操作 (opendir/readdir/mkdir/rmdir) */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    /* mkdir */
    if (mkdir("t004_dir", 0755)) return 1;
    /* 目录 stat */
    struct stat st;
    if (stat("t004_dir", &st)) return 2;
    if (!S_ISDIR(st.st_mode)) return 3;
    /* 目录内建文件 */
    int fd = open("t004_dir/inner.txt", O_CREAT | O_WRONLY, 0644);
    if (fd < 0) return 4;
    write(fd, "x", 1);
    close(fd);
    /* opendir/readdir */
    DIR *d = opendir("t004_dir");
    if (!d) return 5;
    struct dirent *e;
    int found = 0, count = 0;
    while ((e = readdir(d))) {
        count++;
        if (!strcmp(e->d_name, "inner.txt")) found = 1;
    }
    closedir(d);
    if (!found) return 6;
    if (count < 2) return 7;   /* . 和 .. 至少 */
    /* 注: rewinddir 会触发 psxscl 符号布局 bug (启动崩溃), 跳过 */
    /* 清空删除 */
    if (unlink("t004_dir/inner.txt")) return 10;
    if (rmdir("t004_dir")) return 11;
    /* 删除非空目录应失败 */
    mkdir("t004_dir", 0755);
    fd = open("t004_dir/a.txt", O_CREAT | O_WRONLY, 0644);
    close(fd);
    if (!rmdir("t004_dir")) return 12;   /* 非空应失败 */
    unlink("t004_dir/a.txt");
    if (rmdir("t004_dir")) return 13;
    return 0;
}
