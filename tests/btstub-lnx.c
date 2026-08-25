/* btstub-lnx.c — 手动链接流程的 -bt 运行时数据块 (复刻 tccelf.c:tcc_add_btstub)
 *
 * 常规 tcc 链接在 tcc_add_runtime() (!nostdlib 分支) 里自动生成: ___rt_info
 * 数据块 (指向 .stab/.stabstr 调试节的指针数组) + __bt_init_rt 构造函数
 * (调 __bt_init) + __bt_exit_rt 析构. 本仓库 linux 测试走 -nostdlib 手动链接,
 * 该分支整体跳过, 故在此等价复刻. tcc ELF 链接器为 SHF_ALLOC 节自动定义
 * __start_<name>/__stop_<name> (tccelf.c), .stab 因此可得起止地址.
 */
#include <stddef.h>

typedef unsigned long addr_t;   /* rt_context 里 addr_t 即此 */

extern char __start_stab[], __stop_stab[], __start_stabstr[];

struct rt_info_block {
    void *stab_sym;      /* [0]  .stab 起点             */
    void *stab_sym_end;  /* [1]  .stab 终点 (stop)      */
    void *stab_str;      /* [2]  .stabstr               */
    void *esym_start;    /* [3]  ELF 不填 .btsym → 0    */
    void *esym_end;      /* [4]  0                      */
    void *elf_str;       /* [5]  0                      */
    addr_t prog_base;    /* [6]  非 PIE 静态: 0         */
    void *bounds_start;  /* [7]  无 -b: 0               */
    void *top_func;      /* [8]  __bt_init 填入 main    */
    void *next;          /* [9]  运行时链               */
    int num_callers;     /* [10] 默认回溯层数 6         */
    int dwarf;           /* [11] 0 = stabs              */
};

static struct rt_info_block __rt_info = {
    (void *)__start_stab, (void *)__stop_stab, (void *)__start_stabstr,
    0, 0, 0, 0, 0, 0, 0, 6, 0
};

__attribute__((constructor))
static void __bt_init_rt(void)
{
    extern void __bt_init(void *, int);
    __bt_init(&__rt_info, 1);
}

__attribute__((destructor))
static void __bt_exit_rt(void)
{
    extern void __bt_exit(void *);
    __bt_exit(&__rt_info);
}
