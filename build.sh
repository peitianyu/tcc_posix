#!/bin/bash
# build.sh - tcc_posix 完整编译链一键构建
#
# 产物:
#   build/tcc-win.exe     Windows 目标 TCC (PE)
#   build/tcc-linux.exe   Linux 目标 TCC (ELF)
#   build/win-musl-obj/   Windows musl libc.a + crt + 后端
#   build/linux-musl-obj/ Linux musl libc.a + crt
#   lib/libc-win.a        固化 Windows musl libc (install.sh 消费)
#   include/              固化 Linux musl 头 (test.sh -linux 消费)
set -u
BASE="$(cd "$(dirname "$0")" && pwd)"
BOOT_TCC=/d/work/tinycc/win32/tcc.exe   # 自举用宿主 TCC (仅第一次需要)

echo "=== [1/4] 自举 TCC (Windows + Linux 目标) ==="
# 最小必要编译选项 (逐一验证, 见 docs/simd-standard.md §9 与 install.sh 注释):
#   tcc-win.exe: CONFIG_TCC_POSIX=1(默认链 ELF libc.a 而非 msvcrt) + CONFIG_TCC_PREDEFS=1
#     (编译期嵌入 tccdefs_.h, 自足无需外部 tccdefs.h)。ONE_SOURCE 不必传 (tcc.c 默认 1)。
#   tcc-linux.exe: 另加 TCC_TARGET_X86_64 (防御性: 宿主即 x86_64 时自动定义, 显式传保证
#     交叉构建也不受宿主架构影响)。
[ -f "$BASE/build/tcc-win.exe" ] || (cd "$BASE" && "$BOOT_TCC" @build/selfhost-win.list)
[ -f "$BASE/build/tcc-linux.exe" ] || (cd "$BASE" && "$BOOT_TCC" @build/selfhost-linux.list)
"$BASE/build/tcc-win.exe" -v && "$BASE/build/tcc-linux.exe" -v
# 宿主 TCC 运行时 (并入 libc.a 用; 后续可自产)
mkdir -p "$BASE/lib"
[ -f "$BASE/lib/libtcc1.a" ] || cp "$(dirname "$BOOT_TCC")/lib/libtcc1.a" "$BASE/lib/"

echo "=== [2/4] Windows 后端 (psxscl/ntapi/pemagine/dalist) ==="
bash "$BASE/script/build_psxscl.sh" && bash "$BASE/script/build_ntapi.sh" \
	&& bash "$BASE/script/build_pemagine.sh" && bash "$BASE/script/build_dalist.sh"

echo "=== [3/4] musl libc (Windows + Linux) ==="
bash "$BASE/script/build_musl.sh" && bash "$BASE/script/build_musl_linux.sh"

echo "=== [4/4] 固化 include/ 与 lib/libc-win.a ==="
# Linux musl 头 (test.sh -linux 用 -I include/ 编译)
rm -rf "$BASE/include" && mkdir -p "$BASE/include"
cp -r "$BASE/src/posix/musl-1.1.11/include/"* "$BASE/include/"
cp -r "$BASE/build/linux-musl-inc/bits" "$BASE/include/"
cp "$BASE/build/linux-musl-inc/version.h" "$BASE/include/"
# Windows musl libc (install.sh 消费): 固化 build_musl.sh 产物 → lib/libc-win.a
# (2026-08-25 补: 此前 build.sh 注释误称"无需固化", 改 musl 源后 install.sh 仍
# 链接旧 libc-win.a — 断点)。Linux libc 由 build_musl_linux.sh 直用
# build/linux-musl-obj/libc.a, 无需固化。
cp "$BASE/build/win-musl-obj/libc.a" "$BASE/lib/libc-win.a"

echo "=== 完成 ==="
ls -la "$BASE/lib/"*.a | awk '{print "  ", $5, $9}'
echo "日常编译: build/tcc-win.exe -platform=win hello.c    (→ hello.exe)"
echo "          build/tcc-win.exe -platform=linux hello.c  (→ hello_linux)"
