/* emutls 运行时 (linux x86_64): tcc 的 `__thread` 在引用处生成
 * `__emutls_get_address(&__emutls_v_<name>)` 调用, 本函数物化各 TLS 对象。
 *
 * 单线程懒分配 (与 nt64 crt_tls.c 同语义, 纯 C 无平台依赖):
 *   `offset` 是进程级布局游标; 存储落在自有的静态 region, 不触碰 musl 内部
 *   TLS 块。多线程 __thread (per-thread 副本) 超出范围; 单线程内完全正确。
 *
 * 描述符布局与 tccgen.c (decl_initializer VT_TLS 分支) 一致:
 *   { u32 size; u32 align; intptr offset; void *defval } — 24 字节, 8 对齐。
 */
#include <stdint.h>
#include <string.h>

struct __emutls_object {
	uint32_t size;
	uint32_t align;
	intptr_t offset;      /* 0 = 未分配 (哨兵); 分配后 = region 内偏移 */
	void *defval;
};

void *__emutls_get_address(struct __emutls_object *obj)
{
	static char region[1 << 16];   /* 64KiB thread-local area */
	static size_t used = 1;        /* offset 0 留作 "未分配" 哨兵 */

	if (!obj->offset) {
		size_t align = ((size_t)obj->align & 0xFFFF);
		size_t off;
		if (align < 1)
			align = 1;
		off = (used + align - 1) & ~(align - 1);
		obj->offset = (intptr_t)off;
		used = off + obj->size;
		if (obj->defval) {
			char *p = region + off;
			size_t i;
			for (i = 0; i < obj->size; i++)
				p[i] = ((char *)obj->defval)[i];
		} else {
			memset(region + off, 0, obj->size);
		}
	}
	return region + obj->offset;
}
