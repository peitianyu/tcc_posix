/* 测试: R7 statvfs/fstatfs. __sysvtbl[137/138] 注册后不再 ENOSYS;
 * NTFS 卷属性映射到 struct statfs/statvfs.
 * 注: "/" 虚拟根无卷信息 (ENXIO), 用实际操作目录/自建文件验证. */
#include <stdio.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
int main(void) {
    struct statfs  fs  = {0};
    struct statvfs vfs = {0};
    int fail = 0;

    if (statfs(".", &fs) != 0) {
        perror("statfs"); fail++;
    } else if (fs.f_blocks == 0) {
        printf("  FAIL: statfs f_blocks=0\n"); fail++;
    } else {
        printf("statfs: bsize=%lu blocks=%lu bfree=%lu namelen=%lu\n",
               (unsigned long)fs.f_bsize, (unsigned long)fs.f_blocks,
               (unsigned long)fs.f_bfree, (unsigned long)fs.f_namelen);
    }

    if (statvfs(".", &vfs) != 0) {
        perror("statvfs"); fail++;
    } else if (vfs.f_blocks == 0) {
        printf("  FAIL: vfs.f_blocks=0\n"); fail++;
    } else {
        printf("statvfs: frsize=%lu blocks=%lu bfree=%lu namemax=%lu\n",
               (unsigned long)vfs.f_frsize, (unsigned long)vfs.f_blocks,
               (unsigned long)vfs.f_bfree, (unsigned long)vfs.f_namemax);
    }

    /* fstatfs 用自建文件 */
    int fd = open("t037.tmp", O_CREAT|O_RDWR, 0644);
    if (fd < 0) { perror("open"); fail++; }
    else {
        struct statfs  ffs = {0};
        struct statvfs fv  = {0};
        if (fstatfs(fd, &ffs) != 0) {
            perror("fstatfs"); fail++;
        } else if (ffs.f_bavail == 0) {
            printf("  FAIL: fstatfs f_bavail=0\n"); fail++;
        } else {
            printf("fstatfs ok: blocks=%lu bavail=%lu\n",
                   (unsigned long)ffs.f_blocks, (unsigned long)ffs.f_bavail);
        }
        if (fstatvfs(fd, &fv) != 0) {
            perror("fstatvfs"); fail++;
        } else if (fv.f_bfree == 0) {
            printf("  FAIL: fstatvfs f_bfree=0\n"); fail++;
        } else {
            printf("fstatvfs ok: bfree=%lu\n", (unsigned long)fv.f_bfree);
        }
        close(fd);
        unlink("t037.tmp");
    }

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("statfs ok\n");
    return 0;
}