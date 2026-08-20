/* ------------------------------------------------------------- */
/* support for tcc_run() - tcc_posix variant                        */
/*                                                               */
/* Original (lib/runmain.c) gates _runmain behind !_WIN32, since */
/* the PE build used __runmain from libtcc1's crt1.o.  tcc_posix */
/* removed the msvcrt-era crt1.o, so provide _runmain directly.  */
/* ------------------------------------------------------------- */

#ifdef __leading_underscore
# define _(s) s
#else
# define _(s) _##s
#endif

extern void (*_(_init_array_start)[]) (int argc, char **argv, char **envp);
extern void (*_(_init_array_end)[]) (int argc, char **argv, char **envp);
static void run_ctors(int argc, char **argv, char **env)
{
    int i = 0;
    while (&_(_init_array_start)[i] != _(_init_array_end))
        (*_(_init_array_start)[i++])(argc, argv, env);
}

extern void (*_(_fini_array_start)[]) (void);
extern void (*_(_fini_array_end)[]) (void);
static void run_dtors(void)
{
    int i = 0;
    while (&_(_fini_array_end)[i] != _(_fini_array_start))
        (*_(_fini_array_end)[--i])();
}

static void *rt_exitfunc[32];
static void *rt_exitarg[32];
static int __rt_nr_exit;

void __run_on_exit(int ret)
{
    int n = __rt_nr_exit;
    while (n)
	--n, ((void(*)(int,void*))rt_exitfunc[n])(ret, rt_exitarg[n]);
}

int on_exit(void *function, void *arg)
{
    int n = __rt_nr_exit;
    if (n < 32) {
	rt_exitfunc[n] = function;
	rt_exitarg[n] = arg;
        __rt_nr_exit = n + 1;
        return 0;
    }
    return 1;
}

int atexit(void (*function)(void))
{
    return on_exit(function, 0);
}

typedef struct rt_frame {
    void *ip, *fp, *sp;
} rt_frame;

__attribute__((noreturn)) void __rt_exit(rt_frame *, int);

void exit(int code)
{
    rt_frame f;
    run_dtors();
    __run_on_exit(code);
    f.fp = 0;
    f.ip = exit;
    __rt_exit(&f, code);
}

int main(int, char**, char**);

/* tcc_posix -run: 纯计算语义。
   musl 链的完整初始化 (psx_init daemon 线程等) 依赖正常 PE 启动环境,
   tcc_run 内存执行无法提供 → malloc/write/printf 不可用; 纯函数
   (strlen/memcpy 等) 与无 libc 依赖的程序正常。 */
int _runmain(int argc, char **argv, char **envp)
{
    int ret;
    run_ctors(argc, argv, envp);
    ret = main(argc, argv, envp);
    run_dtors();
    __run_on_exit(ret);
    return ret;
}
