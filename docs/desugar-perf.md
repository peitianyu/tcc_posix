# 脱糖性能对照: tcc (验证前端) vs clang -O3 (正式产物)

> 2026-08-23 实测于 `build/simd_bench.c`.
> 目的: 验证"脱 sugar→标准C→clang -O3"管线在计算密集场景下的性能优势,
> 即文档 desugar.md 中的核心卖点(补 TCC 无 AVX/FMA 短板, 正式产物吃满硬件)。

## 基准内容

`build/simd_bench.c` — 对 `v4f`(`__m128` 侧)做大规模向量点积累加:

- `N = 1<<22`(每轮 4M float, 16MB 双缓冲), `ITER = 200` 轮
- 每次内聚处理 4 个 `float`, 4 个独立累加器(ILP), 只输出最终校验和 `s`
- 原生运算符 `acc = acc + x0*y0`(TCC 侧 → addps; clang 侧 `__m128` 原生 → vmulps/vaddps/vfmadd)
- 64 字节对齐数组 + `_mm_load_ps/_mm_store_ps`, 全 16 字节对齐无崩访

## 测量方法

| 侧 | 命令 | 计时 |
|---|---|---|
| tcc 验证前端 | `tcc-win.exe -run simd_bench.c`(原生执行, 含 -I 三头路径) | PowerShell `Get-Date` 差值 |
| clang 正式产物 | `--emit-c` 脱糖 → `clang -O3 -mavx2 -mfma simd_bench.desug.c`(WSL) → 运行 | bash `time` |

两链输出校验和必须一致(见下)。

## 结果

| 指标 | tcc (验证前端) | clang -O3 (正式产物) | 倍率 |
|---|---|---|---|
| 运行耗时 | 2.264 s | ≈ 0.06 s(0.055~0.063) | **≈ 37×** |
| 校验和 `s` | 13420934987776.000000 | 13420934987776.000000 | 一致 |
| N / ITER | 4194304 / 200 | 4194304 / 200 | — |

- 脱糖产物编译(clang 侧)极快: ~0.05 s(单文件, 无链接库负担)。
- 数字完全一致, 说明脱糖管线(透传 + 改写)无语义偏差。

## 结论与意义

1. **性能优势证实**: clang -O3 + AVX2/FMA 天然吃满向量硬件,
   相对 tcc 标量 SSE 提速 ~37× —— 这就是 desugar.md 立项要拿的收益。
2. **脱糖零成本**: `v4f a+b` 原样透传, 不改写为 `_mm_add_ps`; clang 侧 `__m128`
   原生运算符直接编成 `vaddps/vfmadd`, 无中间抽象开销。
3. **验证分工成立**: tcc 用于秒级语义/内存治理迭代, clang 用于最终高性能产物;
   同一份源码、两条编译链、数字一致。

## 复现

```sh
# 1) 数值一致校验(desugar.ps1 已内含): host tcc --emit-c + WSL clang -O3 编译运行 + 比对
powershell -File script\desugar.ps1 build\simd_bench.c        # → PASS simd_bench

# 2) 生成脱糖产物
build\tcc-linux-native.exe --emit-c build\simd_bench.c -o build\desugar\simd_bench.desug.c

# 3) clang -O3 计时(WSL)
clang -O3 -mavx2 -mfma -I include build/desugar/simd_bench.desug.c -o build/desugar/simd_bench.bench
time ./build/desugar/simd_bench.bench

# 4) tcc 计时(host, 对照)
time tcc-win.exe -run simd_bench.c
```