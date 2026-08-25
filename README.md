# tcc_posix — 纯 POSIX 双平台编译链

同一份 POSIX C 源码,编译出 Windows 原生 exe 与 Linux 静态 ELF,两边行为一致。
基于 TCC 0.9.28rc + musl libc + midipix psxscl(musl-libc.org / midipix 2015)。

> 目录导航:本 README 是**一页总览 + 模块索引**。具体设计见 `docs/`(见文末表格),
> 坑与边界见 [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)、待办见
> [docs/TODO.md](docs/TODO.md)、历史见 [docs/RELEASE.md](docs/RELEASE.md)。

---

## 快速开始

```bash
./install.sh                          # 生成自足目录 bin/ (tcc.exe + libc.a + musl 头)
bin/tcc.exe hello.c -o hello.exe      # 一条命令, 默认链 musl (零 Windows API 依赖)
```

## 构建

```bash
./build.sh                            # 从源码完整重建 (自举 TCC + 后端 + musl libc)
build/tcc-win.exe examples/hello.c              # Windows exe (默认平台)
build/tcc-win.exe -platform=linux examples/hello.c  # Linux ELF
```

## 测试

```bash
./test.sh              # Windows 编译+运行
./test.sh -run         # 追加 tcc -run 模式
./test.sh -linux       # 追加 Linux (WSL) 测试
```

当前 **51/51 通过** (test.sh, Windows tcc 自编译运行)：stdio/malloc/mmap/文件/目录/
时间/宽字符/信号/tmp 映射、pthread 全套、线程压力、`-run`(真阻塞 futex)、ctype/
setjmp/regex/search/fenv/multibyte/crypt/prng、语言扩展 defer/model(含常量参)/operator/
reflect/SIMD、cpu-prof、及系统型回归 t033-t045(socket/process/select/termios/dlfcn/
timer/mq/ipc/aio…)。完整矩阵见 [docs/system-modules.md](docs/system-modules.md)。

**扩展语法 clang 闭环** (desugar.ps1)：`--emit-c` 脱糖产物交 WSL clang -O3 编译运行,
与 tcc -run 输出逐字节比对 —— **26 通过 / 1 失败**(t046 为 tcc 特制 SIMD 语法边界,
见 [docs/desugar.md](docs/desugar.md) §4.5)；本例覆盖 defer 早退/model 泛型/operator/
reflect 脱糖/STL 容器与抽象迭代器。

---

## 架构总览

```
源码(POSIX C + 扩展) ──┬─► tcc 魔改前端: -run / -b -bt 秒级迭代验证
                       └─► --emit-c 脱糖输出标准 C ─► clang/LLVM -O3 正式产物
```

- **两大编译产物链**: Windows native exe / Linux static ELF,行为一致。
- **双 libc 支撑**: musl libc + midipix psxscl (POSIX syscall → ntdll 直通)。
- **纯 musl 编译器**: `bin/tcc.exe` 自身按 CONFIG_TCC_MUSL 自举,PE 导入表为空,
  零 winapi 依赖(映射表见 [docs/features.md](docs/features.md) §3)。
- **开发-验证 + 正式产物**: TCC 做验证前端,clang/LLVM 出高性能产物——脱糖输出
  标准 C,吃满 AVX/FMA(性能实测 ≈37×,见 [docs/desugar-perf.md](docs/desugar-perf.md))。

---

## 特性模块索引

| 主题 | 一句话 | 详见 |
|---|---|---|
| 调试 `-b -bt` | 越界/堆错误报 `文件:行`,`-run -b` 透明回退临时 exe | features §1, system-modules R13 |
| TLS | `__thread` via emutls(懒分配 + 物化) | features §2 |
| 内存治理 | `tcc_release`/mmap 登记/arena epoch/逃逸/memtrack 泄漏明细 + 输出 sink | memory-governance.md |
| 系统模块 | socket/process/statfs/pwdgrp/select/termios/dlfcn/timer/mq/ipc/aio | system-modules.md |
| CPU 周期插桩 | `rdtsc` 插桩,总周期/次数/avg 归因到函数 | cpu-prof.md |
| 语言扩展 | defer / model 泛型 / operator / `__builtin_reflect` | features §4, reflect.md |
| SIMD | `v4f` 原生运算符 + `_mm_*` SSE 内建(x86_64-simd 模块) | features §4.3 |
| 脱糖输出 | `--emit-c` 标准 C → clang/LLVM 正式产物(≈37×) | desugar.md, desugar-perf.md |
| `@listfile` | `tcc @build.txt` 包管理 + glob + `%if` 编译选择 | listfile.md |

### 语言扩展速览 (详见 features.md §4)

```c
/* defer — Go 式作用域清理 */
{ struct File f = open_file("x"); defer f.close(); }

/* model — 编译期类型工厂 */
model struct Array(T) { T *data; int len; };
model (T) T max2(T a, T b) { return a > b ? a : b; }
Array(float) a = { buf, 3 };   if (max2(int)(3, 7) != 7) ...

/* operator — 编译期静态分派的算术重载 (零运行时开销) */
struct Vec3 operator+ (struct Vec3 a, struct Vec3 b) { ... }
struct Vec3 c = a + b;   struct Vec3 e = a + b*b;

/* SIMD 原生运算符 */
v4f c = a + b;                    /* addps */

/* 结构体反射 */
const struct __refl *r = __builtin_reflect(struct Vec3);
```

---

## 文档导航 (docs/)

| 文件 | 内容 |
|---|---|
| [features.md](docs/features.md) | 特性深水区: 调试 / TLS / 纯 musl 编译器 / 语言扩展详细设计 |
| [system-modules.md](docs/system-modules.md) | 系统模块可用性矩阵 + 实现路线图 + 决策记录 |
| [memory-governance.md](docs/memory-governance.md) | 内存 5 层治理 / `-b -bt` 捕获清单 / 编程规约 |
| [desugar.md](docs/desugar.md) | `--emit-c` 脱糖管线设计 + 规则表 + Roadmap |
| [desugar-perf.md](docs/desugar-perf.md) | tcc vs clang -O3 性能对照 (≈37×) |
| [reflect.md](docs/reflect.md) | `__builtin_reflect` 设计 |
| [cpu-prof.md](docs/cpu-prof.md) | rdtsc 周期插桩设计 |
| [listfile.md](docs/listfile.md) | `@build.txt` 编译描述 |
| [matrix-library.md](docs/matrix-library.md) | 矩阵库 demo 与扩展决策链(附录 B/C) |
| [opt.md](docs/opt.md) | 优化 |
| [KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知限制 / 未解决问题 (bug, pty/fenv/termios-E2E 等) |
| [TODO.md](docs/TODO.md) | 待办与路线图 (P1/P2) |
| [RELEASE.md](docs/RELEASE.md) | 里程碑变更记录 |

另见 [docs/superpowers](docs/superpowers): 语言扩展的 plan/spec 设计文档。