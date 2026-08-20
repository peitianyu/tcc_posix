#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
int main(void) {
    printf("hello from tcc_posix dual-platform\n");
    printf("strlen: %d\n", (int)strlen("posix"));
    char buf[64];
    getcwd(buf, sizeof buf);
    printf("cwd: %s\n", buf);
    DIR *d = opendir(".");
    if (d) { printf("opendir ok\n"); closedir(d); }
    return 0;
}
