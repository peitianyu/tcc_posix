/* 测试: R9 pwd/grp 探针. musl 库函数路径, 读 /etc/passwd|group.
 * 判断 /etc 是否被映射; 未映射则 getpwuid/getpwnam 返回 NULL. */
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>
int main(void) {
    int fail = 0;
    uid_t uid = getuid();
    printf("getuid=%lu\n", (unsigned long)uid);

    struct passwd *p = getpwuid(uid);
    if (!p)
        printf("getpwuid(%lu)=NULL (errno=%d) -> /etc/passwd 未映射\n",
               (unsigned long)uid, errno);
    else
        printf("getpwuid -> name=%s uid=%lu gid=%lu dir=%s shell=%s\n",
               p->pw_name, (unsigned long)p->pw_uid,
               (unsigned long)p->pw_gid, p->pw_dir, p->pw_shell);

    struct passwd *r = getpwnam("root");
    printf("getpwnam(root)=%s\n", r ? r->pw_name : "NULL");

    struct group *g = getgrgid(getgid());
    printf("getgrgid(%lu)=%s\n", (unsigned long)getgid(),
           g ? g->gr_name : "NULL");

    /* 文件存在性无关紧要, 探针只记录返回 */
    printf("end\n");
    return 0;
}