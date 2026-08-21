/* 测试: search 库 (hsearch/tsearch/lsearch) */
#include <stdio.h>
#include <string.h>
#include <search.h>

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* tsearch/tfind/tdelete 的 cmp 收到的是数据指针本身 (非指向元素的指针) */
static int cmp_key(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

/* lsearch 的 cmp 参数: musl 传 compar(&element, key) (与 glibc 的 (key, &element) 相反!) */
static int cmp_ls(const void *el, const void *key)
{
	return strcmp(*(const char *const *)el, (const char *)key);
}

int main(void) {
	/* --- hsearch 哈希表 --- */
	if (!hcreate(16)) { printf("FAIL hcreate\n"); return 1; }
	ENTRY e, *res;
	e.key = "apple";  e.data = (void *)1;
	if (!hsearch(e, ENTER)) { printf("FAIL enter apple\n"); return 2; }
	e.key = "banana"; e.data = (void *)2;
	if (!hsearch(e, ENTER)) return 3;
	e.key = "apple";
	res = hsearch(e, FIND);
	if (!res || res->data != (void *)1) { printf("FAIL find apple\n"); return 4; }
	e.key = "cherry";
	if (hsearch(e, FIND)) { printf("FAIL 不应找到 cherry\n"); return 5; }
	/* ENTER 已存在的 key 返回旧条目 (不覆盖) */
	e.key = "apple"; e.data = (void *)9;
	res = hsearch(e, ENTER);
	if (!res) return 6;
	e.key = "apple";
	res = hsearch(e, FIND);
	if (res->data != (void *)1) { printf("FAIL enter 覆盖了旧值\n"); return 7; }
	hdestroy();

	/* --- tsearch 二叉搜索树 --- */
	{
		void *root = NULL;
		char k1[] = "key1", k2[] = "key2", k3[] = "key3";
		if (!tsearch(k1, &root, cmp_key)) return 8;
		if (!tsearch(k2, &root, cmp_key)) return 9;
		if (!tsearch(k3, &root, cmp_key)) return 10;
		/* tfind 查找: musl 返回节点指针 (非 key 指针, 与 glibc 语义不同),
		 * 只能验证存在性 */
		void *f = tfind(k2, &root, cmp_key);
		if (!f) { printf("FAIL tfind key2\n"); return 11; }
		if (tfind("nope", &root, cmp_key)) { printf("FAIL tfind 不应命中\n"); return 12; }
		/* tdelete */
		if (!tdelete(k1, &root, cmp_key)) { printf("FAIL tdelete key1\n"); return 13; }
		if (tfind(k1, &root, cmp_key)) { printf("FAIL key1 删除后仍在\n"); return 14; }
		if (!tfind(k2, &root, cmp_key)) { printf("FAIL key2 应还在\n"); return 15; }
		/* twalk 遍历计数 */
	}

	/* --- lsearch 线性搜索 (找不到则追加) --- */
	{
		char *arr[8];
		size_t n = 0;
		arr[n++] = (char *)"alpha";
		arr[n++] = (char *)"beta";
		/* 找到 */
		void *f = lsearch("beta", arr, &n, 8, cmp_ls);
		if (!f || n != 2) { printf("FAIL lsearch 找到\n"); return 16; }
		/* 找不到 → 追加 */
		f = lsearch("gamma", arr, &n, 8, cmp_ls);
		if (!f || n != 3) { printf("FAIL lsearch 追加 n=%d\n", (int)n); return 17; }
		if (strcmp((char *)f, "gamma")) return 18;
	}
	printf("PASS\n");
	return 0;
}
