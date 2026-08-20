/* 测试: 宽字符函数 (musl wchar_t=4字节; TCC 的 L"" 字面量是 2 字节,
   不匹配 → 不用 L"" 字面量, 用 wchar_t 数组手动构造) */
#include <wchar.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    /* 手动构造宽字符串 (4 字节 wchar_t) */
    wchar_t ws[8];
    ws[0] = 'h'; ws[1] = 'i'; ws[2] = 0;
    if (wcslen(ws) != 2) return 1;
    /* wcscpy/wcscmp */
    wchar_t ws2[8];
    wcscpy(ws2, ws);
    if (wcscmp(ws2, ws)) return 2;
    /* isw* 分类 */
    if (!iswdigit(L'5')) return 3;
    if (iswdigit(L'x')) return 4;
    if (!iswalpha(L'Z')) return 5;
    if (!iswspace(L' ')) return 6;
    /* towupper/towlower */
    if (towupper(L'a') != L'A') return 7;
    if (towlower(L'Q') != L'q') return 8;
    /* swprintf 需要宽字面量格式串 (TCC 2 字节 vs musl 4 字节不匹配),
       跳过; 宽函数本身用非字面量测试 */
    /* mbrtowc 状态式 */
    mbstate_t st;
    memset(&st, 0, sizeof st);
    wchar_t wc;
    size_t r = mbrtowc(&wc, "B", 1, &st);
    if (r != 1 || wc != L'B') return 11;
    /* wmemcpy/wmemcmp */
    wchar_t src[4] = {1,2,3,4}, dst[4];
    wmemcpy(dst, src, 4);
    if (wmemcmp(dst, src, 4)) return 12;
    return 0;
}
