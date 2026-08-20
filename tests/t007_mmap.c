/* 测试: mmap (匿名/文件映射/protect) */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    /* 匿名映射 */
    void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return 1;
    memset(p, 0x42, 4096);
    if ((unsigned char)((char*)p)[0] != 0x42) return 2;
    if ((unsigned char)((char*)p)[4095] != 0x42) return 3;
    /* mprotect 只读 (psxscl 页对齐修复后可用) */
    if (mprotect(p, 4096, PROT_READ)) return 4;
    if ((unsigned char)((char*)p)[0] != 0x42) return 5;
    if (munmap(p, 4096)) return 6;
    /* 文件映射 */
    int fd = open("t006_mmap.bin", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) return 7;
    char data[8192];
    memset(data, 'M', sizeof data);
    if (write(fd, data, sizeof data) != sizeof data) return 8;
    void *m = mmap(NULL, sizeof data, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) return 9;
    if (((char*)m)[0] != 'M' || ((char*)m)[8191] != 'M') return 10;
    /* 写回文件 (msync/munmap) */
    ((char*)m)[0] = 'X';
    if (msync(m, sizeof data, MS_SYNC)) return 11;
    if (munmap(m, sizeof data)) return 12;
    /* 验证写回 */
    char chk[8192];
    if (lseek(fd, 0, SEEK_SET) != 0) return 13;
    if (read(fd, chk, sizeof chk) != sizeof chk) return 14;
    if (chk[0] != 'X' || chk[8191] != 'M') return 15;
    close(fd);
    unlink("t006_mmap.bin");
    /* 大映射 (4MB) */
    void *big = mmap(NULL, 4 * 1024 * 1024, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (big == MAP_FAILED) return 16;
    memset(big, 0x7F, 4 * 1024 * 1024);
    if ((unsigned char)((char*)big)[4 * 1024 * 1024 - 1] != 0x7F) return 17;
    munmap(big, 4 * 1024 * 1024);
    return 0;
}
