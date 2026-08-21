/* 测试: fenv 浮点环境 API 契约
 * 注意: nt64 的 fenv 是 dummy 实现 (fenv.c 注释 "Dummy functions for archs
 * lacking fenv implementation"), 故此处只测双平台一致的 API 契约
 * (返回值/参数校验), 真实舍入与异常检测语义待 nt64 fenv 补全后加强。
 */
#include <stdio.h>
#include <fenv.h>

int main(void) {
	/* 默认舍入模式: FE_TONEAREST */
	if (fegetround() != FE_TONEAREST) { printf("FAIL 默认舍入\n"); return 1; }
	/* 设置合法舍入模式返回 0 */
	if (fesetround(FE_TONEAREST)) return 2;
	if (fesetround(FE_UPWARD)) { printf("FAIL fesetround(FE_UPWARD)\n"); return 3; }
	if (fesetround(FE_DOWNWARD)) return 4;
	if (fesetround(FE_TOWARDZERO)) return 5;
	fesetround(FE_TONEAREST);
	/* 非法舍入模式返回 -1 (fesetround.c 参数检查, dummy 也走) */
	if (fesetround(0x4000) != -1) { printf("FAIL 非法舍入未报错\n"); return 6; }
	/* 异常 flag 契约 */
	if (feclearexcept(FE_ALL_EXCEPT)) return 7;
	if (fetestexcept(FE_ALL_EXCEPT) != 0) { printf("FAIL 清空后残留\n"); return 8; }
	/* feholdexcept: 保存环境并清空, 返回 0 */
	fenv_t env;
	if (feholdexcept(&env)) return 9;
	/* feupdateenv: 恢复并合并 */
	if (feupdateenv(&env)) return 10;
	/* fegetexceptflag/fesetexceptflag 往返 */
	{
		fexcept_t fl = 0;
		if (fegetexceptflag(&fl, FE_ALL_EXCEPT)) return 11;
		if (fesetexceptflag(&fl, FE_ALL_EXCEPT)) return 12;
	}
	printf("PASS\n");
	return 0;
}
