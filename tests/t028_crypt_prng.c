/* 测试: crypt 口令哈希 + prng 随机数 (确定性重放) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crypt.h>

int main(void) {
	/* --- crypt: 同 salt 确定性, 不同 salt 不同 ---
	 * 注意: crypt() 返回静态缓冲, 连续调用会覆盖, 必须先保存副本 */
	{
		char save[128];
		const char *h1 = crypt("password", "$1$salt1$");
		if (!h1) { printf("FAIL crypt NULL\n"); return 1; }
		strcpy(save, h1);
		const char *h2 = crypt("password", "$1$salt1$");
		if (strcmp(save, h2)) { printf("FAIL crypt 同 salt 不同结果\n"); return 2; }
		const char *h3 = crypt("password", "$1$salt2$");
		if (!strcmp(save, h3)) { printf("FAIL crypt 不同 salt 结果相同\n"); return 3; }
		if (strncmp(save, "$1$", 3)) { printf("FAIL crypt 格式 %s\n", save); return 4; }
		/* sha256 ($5$) 与 sha512 ($6$) 前缀分派 (注意静态缓冲别名, 先存副本) */
		{
			char s5s[128], s6s[128];
			const char *s5 = crypt("password", "$5$saltsalt$");
			if (!s5 || strncmp(s5, "$5$", 3)) return 6;
			strcpy(s5s, s5);
			const char *s6 = crypt("password", "$6$saltsalt$");
			if (!s6 || strncmp(s6, "$6$", 3)) return 7;
			strcpy(s6s, s6);
			if (!strcmp(s5s, s6s)) return 8;
		}
		/* crypt_r 等价 */
		{
			struct crypt_data cd;
			memset(&cd, 0, sizeof cd);
			char *hr = crypt_r("password", "$1$salt1$", &cd);
			if (!hr || strcmp(hr, save)) { printf("FAIL crypt_r\n"); return 9; }
		}
	}

	/* --- rand/srand 确定性重放 --- */
	{
		srand(42);
		int a1[5], i;
		for (i = 0; i < 5; i++) a1[i] = rand();
		srand(42);
		for (i = 0; i < 5; i++)
			if (rand() != a1[i]) { printf("FAIL srand 重放\n"); return 10; }
	}

	/* --- rand48 族 --- */
	{
		double d1[4]; int i;
		srand48(0x1234);
		for (i = 0; i < 4; i++) d1[i] = drand48();
		srand48(0x1234);
		for (i = 0; i < 4; i++)
			if (drand48() != d1[i]) { printf("FAIL srand48 重放\n"); return 11; }
		long l1[3];
		srand48(7);
		for (i = 0; i < 3; i++) l1[i] = lrand48();
		srand48(7);
		for (i = 0; i < 3; i++)
			if (lrand48() != l1[i]) { printf("FAIL lrand48 重放\n"); return 12; }
		unsigned short sd[3] = { 1, 2, 3 };
		seed48(sd);
		long m1 = mrand48(), m2 = mrand48();
		seed48(sd);
		if (mrand48() != m1 || mrand48() != m2) { printf("FAIL seed48 重放\n"); return 13; }
	}

	/* --- rand_r (可重入) --- */
	{
		unsigned int rseed = 99;
		int r1 = rand_r(&rseed);
		rseed = 99;
		if (rand_r(&rseed) != r1) { printf("FAIL rand_r\n"); return 14; }
	}

	/* --- random/srandom --- */
	{
		srandom(7);
		long o1 = random(), o2 = random();
		srandom(7);
		if (random() != o1 || random() != o2) { printf("FAIL srandom 重放\n"); return 15; }
	}
	printf("PASS\n");
	return 0;
}
