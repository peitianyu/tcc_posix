/* 测试: regex 正则引擎 (regcomp/regexec/regerror, TRE) */
#include <stdio.h>
#include <string.h>
#include <regex.h>

static int re_check(const char *pat, int flags, const char *str, int expect)
{
	regex_t re;
	int rc = regcomp(&re, pat, flags);
	if (rc) {
		char eb[128];
		regerror(rc, &re, eb, sizeof eb);
		printf("FAIL regcomp(%s): %s\n", pat, eb);
		return -1;
	}
	rc = regexec(&re, str, 0, NULL, 0);
	regfree(&re);
	if ((rc == 0) != (expect == 0)) {
		printf("FAIL regexec(%s, %s): rc=%d expect=%d\n", pat, str, rc, expect);
		return -2;
	}
	return 0;
}

int main(void) {
	/* 基本: 子串匹配 / 锚定 */
	if (re_check("hello", 0, "say hello world", 0)) return 1;
	if (re_check("^hello$", 0, "hello", 0)) return 2;
	if (re_check("^hello$", 0, "hellox", REG_NOMATCH)) return 3;
	if (re_check("^a.c$", 0, "abc", 0)) return 4;       /* . 通配 */
	if (re_check("^a.c$", 0, "ac", REG_NOMATCH)) return 5;
	/* 字符类 (ERE) */
	if (re_check("^[a-z]+[0-9]+$", REG_EXTENDED, "abc123", 0)) return 6;
	if (re_check("^[a-z]+[0-9]+$", REG_EXTENDED, "abc", REG_NOMATCH)) return 7;
	/* 捕获组 */
	{
		regex_t re;
		regmatch_t pm[4];
		if (regcomp(&re, "^([0-9]+)-([0-9]+)$", REG_EXTENDED)) return 8;
		if (regexec(&re, "2024-08", 4, pm, 0)) { printf("FAIL capture match\n"); return 9; }
		if (pm[1].rm_so != 0 || pm[1].rm_eo != 4) { printf("FAIL pm1 %d-%d\n", (int)pm[1].rm_so, (int)pm[1].rm_eo); return 10; }
		if (pm[2].rm_so != 5 || pm[2].rm_eo != 7) { printf("FAIL pm2 %d-%d\n", (int)pm[2].rm_so, (int)pm[2].rm_eo); return 11; }
		regfree(&re);
	}
	/* REG_ICASE */
	if (re_check("^abc$", REG_ICASE, "ABC", 0)) return 12;
	if (re_check("^abc$", REG_ICASE, "ABc", 0)) return 13;
	/* ERE 量词与分支 */
	if (re_check("^ab+c$", REG_EXTENDED, "abbbc", 0)) return 14;
	if (re_check("^ab+c$", REG_EXTENDED, "ac", REG_NOMATCH)) return 15;
	if (re_check("^a(b|c)d$", REG_EXTENDED, "acd", 0)) return 16;
	if (re_check("^a(b|c)d$", REG_EXTENDED, "abd", 0)) return 17;
	if (re_check("^(ab){2}$", REG_EXTENDED, "abab", 0)) return 18;
	if (re_check("^(ab){2}$", REG_EXTENDED, "ab", REG_NOMATCH)) return 19;
	/* REG_NEWLINE */
	if (re_check("^b$", REG_NEWLINE, "a\nb", 0)) return 20;
	printf("PASS\n");
	return 0;
}
