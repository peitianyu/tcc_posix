/* dlfcn stub (linux x86_64 静态 ELF)。
 *
 * 静态链接的 ELF 没有动态加载器: dlopen 不可用 (musl 静态语义: 返回 NULL),
 * dlsym/dlclose/dladdr 相应 stub。符号必须存在, 因为 tcc 链接用户程序时
 * 要解析所有引用 (即使测试在 SKIP 分支未执行)。
 *
 * 注: build_musl_linux.sh 的 C 编译排除 "*ldso*" 路径 (musl 动态加载器
 * dynlink.c 依赖内部头, 不参与静态构建), 本文件置于 src/dlfcn/ 自动入包。
 */
#define _GNU_SOURCE
#include <dlfcn.h>

void *dlopen(const char *file, int mode)
{
	(void)file; (void)mode;
	return 0;                        /* 静态 ELF: 无动态加载 */
}

void *dlsym(void *handle, const char *name)
{
	(void)handle; (void)name;
	return 0;
}

int dlclose(void *handle)
{
	(void)handle;
	return 0;
}

char *dlerror(void)
{
	return "dlfcn unavailable (static ELF, no dynamic loader)";
}

int dladdr(const void *addr, Dl_info *info)
{
	(void)addr;
	if (info) {
		info->dli_fname = 0;
		info->dli_fbase = 0;
		info->dli_sname = 0;
		info->dli_saddr = 0;
	}
	return 0;
}
