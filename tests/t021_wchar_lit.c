#include <stdio.h>
#include <wchar.h>
#include <string.h>
int main(void) {
	const wchar_t *ws = L"中文宽字符";
	wchar_t c = L'宽';
	wchar_t buf[32];
	printf("wchar_t size=%d\n", (int)sizeof(wchar_t));
	printf("wcslen=%d\n", (int)wcslen(ws));
	wcscpy(buf, ws);
	if (wcslen(buf) != 5) { printf("FAIL len\n"); return 1; }
	if (c != L'宽') { printf("FAIL char\n"); return 1; }
	printf("L literal OK: %ls\n", buf);
	return 0;
}
