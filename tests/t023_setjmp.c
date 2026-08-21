/* 测试: setjmp/longjmp 非局部跳转 (nt64 汇编实现) */
#include <stdio.h>
#include <setjmp.h>

static jmp_buf jb;

static int deep(int n)
{
	char pad[128]; /* 压栈, 验证深层跳转 */
	pad[0] = (char)n;
	if (n > 0) return deep(n - 1) + pad[0];
	longjmp(jb, 42); /* 从 5 层递归深处跳回 */
	return -1;       /* unreachable */
}

int main(void) {
	int r = setjmp(jb);
	if (r == 0) {
		/* 第一次进入: 正常路径 */
		if (deep(5) != -1) { printf("FAIL 应已跳回\n"); return 1; }
		printf("FAIL: 未跳回\n");
		return 2;
	}
	if (r != 42) { printf("FAIL val=%d (期望 42)\n", r); return 3; }
	/* longjmp(jb, 0) → setjmp 应返回 1 (0 被替换) */
	if (setjmp(jb) == 0) {
		longjmp(jb, 0);
		printf("FAIL: 0 跳回未归一\n");
		return 4;
	}
	/* 第二次 setjmp 返回非零后再次 setjmp 仍工作 */
	if (setjmp(jb) == 0) {
		longjmp(jb, 7);
		printf("FAIL\n");
		return 5;
	}
	printf("PASS\n");
	return 0;
}
