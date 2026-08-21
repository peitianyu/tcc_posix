/* t035_socket: socket() 系统调用基础验证
 * 验证 AF_INET/INET6 + STREAM/DGRAM 四种组合 (protocol=0 由 psxscl 推导 TCP/UDP)
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static void chk(const char *name, int fd)
{
	if (fd < 0) {
		printf("FAIL %s: errno=%d (%s)\n", name, errno, strerror(errno));
		return;
	}
	/* fd 分配成功才代表顶层对了; 关闭释放 */
	if (close(fd) != 0)
		printf("WARN %s: close errno=%d\n", name, errno);
	else
		printf("PASS %s -> fd=%d\n", name, fd);
}

int main(void)
{
	chk("socket(AF_INET,SOCK_STREAM,0)", socket(AF_INET, SOCK_STREAM, 0));
	chk("socket(AF_INET,SOCK_DGRAM,0)",  socket(AF_INET, SOCK_DGRAM, 0));
	chk("socket(AF_INET6,SOCK_STREAM,0)",socket(AF_INET6, SOCK_STREAM, 0));
	chk("socket(AF_INET6,SOCK_DGRAM,0)", socket(AF_INET6, SOCK_DGRAM, 0));
	/* 原始套接字: protocol 显式 ICMP, 应返回 EAFNOSUPPORT/EPROTONOSUPPORT 或成功 */
	chk("socket(AF_INET,SOCK_RAW,IPPROTO_ICMP)", socket(AF_INET, SOCK_RAW, IPPROTO_ICMP));
	/* 非法 domain */
	{
		int fd = socket(AF_UNSPEC, SOCK_STREAM, 0);
		if (fd < 0 && errno == EINVAL)
			printf("PASS socket(AF_UNSPEC) -> EINVAL\n");
		else if (fd >= 0) { close(fd); printf("WARN socket(AF_UNSPEC) 返回 %d\n", fd); }
		else
			printf("FAIL socket(AF_UNSPEC): errno=%d (%s)\n", errno, strerror(errno));
	}
	return 0;
}