# tcc_posix — 纯 POSIX 双平台编译链

同一份 POSIX C 源码,编译出 Windows 原生 exe 与 Linux 静态 ELF,两边行为一致。

基于 TCC 0.9.28rc + musl libc + midipix psxscl(musl-libc.org / midipix 2015)。

## 快速开始

```bash
./install.sh                          # 生成自足目录 bin/ (tcc.exe + libc.a + musl 头)
bin/tcc.exe hello.c -o hello.exe      # 一条命令, 默认链 musl (零 Windows API 依赖)
```

```bash
./build.sh                            # 从源码完整重建 (自举 TCC + 后端 + musl libc)
build/tcc-win.exe examples/hello.c              # Windows exe (默认平台)
build/tcc-win.exe -platform=linux examples/hello.c  # Linux ELF
```

## 测试

```bash
./test.sh              # Windows 编译+运行 (46 测试)
./test.sh -run         # 追加 tcc -run 模式
./test.sh -linux       # 追加 Linux (WSL) 测试
```

当前 **46/46 通过** (test.sh, Windows tcc 自编译运行)。覆盖 stdio/malloc/mmap/文件/目录/时间/
宽字符/信号/tmp 映射、pthread 全套 (create/join/mutex/cond/barrier/sem/递归锁)、
线程压力、main 提前退出、tcc -run (含 futex 真阻塞)、ctype/setjmp/regex/search/
fenv/multibyte/crypt/prng 模块 (t022-t028),语言扩展 defer (t029)、
对象方法 (t030) 与 model 泛型 (t031-t032),以及系统型回归 (t033-t045):
sched_yield、ENOSYS 兜底、socket、process、statfs/fstatfs、pwd/grp 虚拟 /etc、
unsupported、select/poll、termios console 映射、dlfcn、timer、mq、System V ipc、aio。

## 系统模块可用性

已实现并通过回归 (详见 docs/system-modules.md):

| 模块 | 状态 |
|---|---|
| socket / network | ✅ t035 |
| process | ✅ t036 |
| statfs / fstatfs | ✅ t037 |
| pwd / grp (虚拟 /etc 映射) | ✅ t038 |
| select / poll | ✅ t040_select_poll |
| termios console 映射 | ✅ t040_termios (E2E 待真 console) |
| dlfcn | ✅ t041 |
| timer | ✅ t042 |
| mq | ✅ t043 |
| System V ipc (msg/sem/shm) | ✅ t044 |
| aio | ✅ t045 (后台线程退出段错误已修复) |
| ENOSYS 兜底 / sched_yield / env PATH | ✅ t034/t039 / t033 / t008 |

## 语言扩展

**defer 语句** (t029):Go 式作用域清理,注册点求值、离开作用域逆序调用,与
`__attribute__((cleanup))` 共用 scope cleanup 机制,支持 return/goto 全路径:

```c
{ struct File f = open_file("x"); defer f.close(); }
```

**对象方法** (t030):C++ 式 —— struct 体内函数定义即方法,无需关键字。隐式 `self`
参数 (类型 `T*`),方法体可直接引用字段 (编译期替换为 `self->字段`),`v.func()` /
`ptr->func()` 自动注入 self。方法编译为内部函数 `__method_<id>_<名>` (static),
查表调用,零运行时开销:

```c
struct Point {
    int x, y;
    int sum(void) { return x + y; }          /* 隐式 self, 直接写字段 */
    void set(int a, int b) { x = a; y = b; }
    int combo(int k) { return self->sum() + self->mul(k); }  /* 方法互调 */
};
struct Point p = { 3, 4 };
p.sum();        /* ≡ __method_0_sum(&p) */
```

已知限制:方法引用的字段须声明在方法之前;方法体局部变量/参数不得与字段同名
(会被替换为 self->字段);方法间互调须 `self->`;方法必须带函数体;self 为方法
保留参数名。

**model 泛型** (t031-t032):编译期类型工厂 —— `model` 关键字定义类型/函数模板,
使用处提供具体类型参数,编译器克隆模板并替换,生成与手写特定类型完全一致的
零开销代码。struct/union 与 function 两类均支持:

```c
model struct Array(T) { T *data; int len; };        /* 结构体模板 */
model (T) T max2(T a, T b) { return a > b ? a : b; } /* 函数模板 (无 function 关键字) */

Array(float) a = { buf, 3 };        /* 实例化: 合成内部类型 Array_float */
if (a.data[1] == 2.5f) ...          /* 字段访问与普通 struct 无异 */
Array(double) b;                    /* 同参缓存复用, sizeof 一致 */
if (max2(int)(3, 7) != 7) ...       /* 实例化调用: 生成内部函数 max2_int */
if (max2(double)(2.5, 1.5) != 2.5) ...
```

实现机制:模板定义仅记录 token 流 (不生成代码);实例化时记录实参 token,
合成内部名 (`Array_float` / `max2_int`) 缓存查重,替换类型参数后重放走标准
解析 (struct_decl / decl)。函数体延迟到文件末尾编译 (避免插入调用方函数)。
支持多类型参数、嵌套实例化 (`Array(Box(int))`)、返回类型为模板实例、泛型内
嵌方法/递归。已知限制:model 模板名与成员/参数名不得重复 (实例化替换会冲突);
函数模板实参必须是类型 (非类型模板参数不支持);模板内方法引用字段需声明在前。

## 已知限制

- **`__thread` / `thread_local` 不可用** — TCC x86-64 只生成 `%fs:TPOFF32`
  (initial-exec, fs 基址 = TLS 块末尾),而 Windows fs 固定指向 TEB,无法指向
  musl TLS 块;修需重写 TCC TLS 代码生成 (TEB 槽/dtv),不支持 TLSGD 动态模型。
- **pty 链路不可用** — psxscl-2015 的 /dev 分派器对 ptmx/pts 显式返回
  NOT_FOUND(存根)。termios console 映射 (TCGETS/TCSETS/TIOCGWINSZ,
  GetConsoleMode/SetConsoleMode) 已实现并通过 t040, 但本自动化环境无 console,
  需在真交互终端跑一次 t040 做 E2E 数值确认。
- **fenv 为 dummy 实现** — nt64 的 fenv.c 注释 "Dummy functions for archs
  lacking fenv implementation",feclearexcept/fetestexcept 等返回 0,真实
  舍入/异常语义不生效 (t026 只测 API 契约)。
- **`-run` 模式** 走完整 musl 链 (非 msvcrt):printf/malloc/time/opendir/pthread
  全套可用,futex 基于 ntdll RtlWaitOnAddress/RtlWakeAddressAll 真阻塞。
