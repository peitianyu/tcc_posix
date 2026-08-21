/* 测试: multibyte 多字节序列 (UTF-8, musl C locale 默认 UTF-8) */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>

int main(void) {
	/* musl 默认 C locale 是 ASCII-only (MB_CUR_MAX=1), UTF-8 需显式启用 */
	if (!setlocale(LC_CTYPE, "C.UTF-8")) { printf("FAIL setlocale C.UTF-8\n"); return 16; }
	/* "中" = E4 B8 AD = U+4E2D */
	const char *zh = "\xE4\xB8\xAD";
	mbstate_t st;
	wchar_t wc = 0;
	size_t n;

	/* --- mbrtowc 一次读全 --- */
	memset(&st, 0, sizeof st);
	n = mbrtowc(&wc, zh, 3, &st);
	if (n != 3 || wc != 0x4E2D) { printf("FAIL mbrtowc n=%d wc=%04X\n", (int)n, (unsigned)wc); return 1; }

	/* --- 逐字节 partial (-2), 末字节返回 1 --- */
	memset(&st, 0, sizeof st);
	wc = 0;
	n = mbrtowc(&wc, zh, 1, &st);
	if (n != (size_t)-2) { printf("FAIL partial1 %d\n", (int)n); return 2; }
	n = mbrtowc(&wc, zh + 1, 1, &st);
	if (n != (size_t)-2) return 3;
	n = mbrtowc(&wc, zh + 2, 1, &st);
	if (n != 1 || wc != 0x4E2D) { printf("FAIL partial3 %d\n", (int)n); return 4; }

	/* --- ASCII 直接 1 字节 --- */
	memset(&st, 0, sizeof st);
	n = mbrtowc(&wc, "A", 1, &st);
	if (n != 1 || wc != L'A') return 5;

	/* --- 非法序列 → -1 + EILSEQ --- */
	memset(&st, 0, sizeof st);
	errno = 0;
	n = mbrtowc(&wc, "\xFF", 1, &st);
	if (n != (size_t)-1 || errno != EILSEQ) { printf("FAIL eilseq n=%d errno=%d\n", (int)n, errno); return 6; }

	/* --- mbstowcs / wcstombs 往返 --- */
	{
		const char *mb = "abc\xE4\xB8\xAD" "def";
		wchar_t wbuf[16];
		n = mbstowcs(wbuf, mb, 16);
		if (n != 7) { printf("FAIL mbstowcs n=%d\n", (int)n); return 7; }
		if (wbuf[0] != L'a' || wbuf[3] != 0x4E2D || wbuf[6] != L'f') return 8;
		char mbuf[32];
		n = wcstombs(mbuf, wbuf, 32);
		if (n != 9 || strcmp(mbuf, mb)) { printf("FAIL wcstombs n=%d\n", (int)n); return 9; }
	}

	/* --- wcrtomb --- */
	{
		char ob[8];
		memset(&st, 0, sizeof st);
		n = wcrtomb(ob, 0x4E2D, &st);
		if (n != 3 || (unsigned char)ob[0] != 0xE4 || (unsigned char)ob[1] != 0xB8 || (unsigned char)ob[2] != 0xAD) {
			printf("FAIL wcrtomb n=%d\n", (int)n); return 10;
		}
	}

	/* --- mbtowc / wctomb (全局状态) --- */
	n = mbtowc(&wc, zh, 3);
	if (n != 3 || wc != 0x4E2D) { printf("FAIL mbtowc %d\n", (int)n); return 11; }
	{
		char ob[8];
		n = wctomb(ob, 0x4E2D);
		if (n != 3 || (unsigned char)ob[0] != 0xE4) return 12;
	}

	/* --- mbrlen / mblen --- */
	memset(&st, 0, sizeof st);
	n = mbrlen(zh, 3, &st);
	if (n != 3) { printf("FAIL mbrlen %d\n", (int)n); return 13; }
	n = mblen(zh, 3);
	if (n != 3) return 14;

	/* --- wcsrtombs 带状态 --- */
	{
		const wchar_t *ws = L"ab\x4E2D";
		char mbuf[16];
		memset(&st, 0, sizeof st);
		n = wcsrtombs(mbuf, &ws, 16, &st);
		if (n != 5 || strcmp(mbuf, "ab\xE4\xB8\xAD")) { printf("FAIL wcsrtombs %d\n", (int)n); return 15; }
	}
	printf("PASS\n");
	return 0;
}
