/* 测试: ctype 字符分类/转换 (isalpha/toupper 等) */
#include <stdio.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
int main(void) {
	/* --- 基本单字节分类 --- */
	if (!isalpha('A')) { printf("FAIL isalpha A\n"); return 1; }
	if (!isalpha('z')) return 2;
	if (isalpha('5')) return 3;
	if (isalpha('!')) return 4;
	if (!isdigit('5') || isdigit('x')) return 5;
	if (!isalnum('9') || !isalnum('a') || isalnum('!')) return 6;
	if (!isspace(' ') || !isspace('\t') || !isspace('\n')) return 7;
	if (isspace('A')) return 8;
	if (!iscntrl('\n') || iscntrl('A')) return 9;
	if (!isupper('A') || isupper('a')) return 10;
	if (!islower('a') || islower('A')) return 11;
	if (!isprint(' ') || isprint('\n')) return 12;
	if (!isgraph('x') || isgraph(' ')) return 13;
	if (!ispunct('!') || ispunct('a')) return 14;
	if (!isxdigit('f') || !isxdigit('F') || !isxdigit('9') || isxdigit('g')) return 15;
	if (!isblank(' ') || !isblank('\t') || isblank('a')) return 16;
	if (!isascii(0x7F) || isascii(0x80)) return 17;
	/* --- 大小写转换 --- */
	if (toupper('a') != 'A' || toupper('Z') != 'Z') return 18;
	if (tolower('Z') != 'z' || tolower('a') != 'a') return 19;
	/* --- 宽字符分类 (Unicode 属性表) --- */
	if (!iswalpha(L'中')) { printf("FAIL iswalpha CJK\n"); return 20; }
	if (!iswalpha(L'A') || iswalpha(L'5')) return 21;
	if (!iswdigit(L'7') || iswdigit(L'x')) return 22;
	if (!iswspace(L' ') || iswspace(L'A')) return 23;
	if (iswupper(L'a') || !iswupper(L'Z')) return 24;
	if (iswlower(L'A') || !iswlower(L'z')) return 25;
	if (!iswprint(L'中') || iswprint(L'\n')) return 26;
	if (!iswxdigit(L'f') || iswxdigit(L'g')) return 27;
	if (towupper(L'a') != L'A' || towupper(L'中') != L'中') return 28;
	if (towlower(L'Z') != L'z' || towlower(L'中') != L'中') return 29;
	printf("PASS\n");
	return 0;
}
