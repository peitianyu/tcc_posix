#!/bin/bash
# install.sh - 生成 tcc_posix Windows 编译链安装 (bin/ 自足目录)
#
# 产物: bin/tcc.exe (开箱即用: tcc hello.c → hello.exe, 默认链 musl + psxscl)
#       bin/include/          musl 头 (nt64)
#       bin/lib/libc.a        3.3M, 唯一运行时文件: musl+chkstk+init_array
#                             +mem4+后端+libtcc1+crt_crt1(入口)+runmain(-run)
#                             (零 Windows API 依赖 → 无 kernel32.def)
set -u
BASE="$(cd "$(dirname "$0")" && pwd)"
BOOT_TCC=tcc.exe

echo "=== [1/3] 自举 tcc-win (CONFIG_TCC_POSIX: 默认链 musl) ==="
# BOOT_TCC must be a full self-hosted x86_64 tcc (e.g. D:/work/tinycc/win32/tcc.exe).
# CONFIG_TCC_PREDEFS=1 embeds tccdefs_.h so the built tcc bootstraps standalone
# (no runtime dependency on an external <tccdefs.h>), for a portable bin/ tree.
# 参数集中在 @build/selfhost-win.list (自举/重自举共用; 纯参数格式兼容 BOOT 基础 @)。
BOOT_TCC="${BOOT_TCC:-tcc.exe}"
(cd "$BASE" && "$BOOT_TCC" @build/selfhost-win.list) 2>&1 | grep -iE 'error' | head -5
"$BASE/build/tcc-win.exe" -v || exit 1

echo "=== [2/3] 组装 bin/ ==="
BIN="$BASE/bin"
rm -rf "$BIN"
mkdir -p "$BIN/lib" "$BIN/include/winapi"

# 编译器
cp "$BASE/build/tcc-win.exe" "$BIN/tcc.exe"

# musl 头 (nt64 全量: include + arch/bits 生成的 alltypes 等)
cp -r "$BASE/src/posix/musl-nt64/include/"* "$BIN/include/" 2>/dev/null
cp -r "$BASE/src/posix/musl-nt64/arch/nt64/bits" "$BIN/include/" 2>/dev/null
# winapi 头 (tcc 编译自身需要 windows.h 等, 从 src 侧 win32/include 拿)
cp -r "$BASE/src/posix/musl-nt64/include/winapi/." "$BIN/include/winapi/" 2>/dev/null || true

# 库 → lib/ (tcc 的 library_paths 含 <tccdir>/lib)
# libc.a 含一切: musl + chkstk + init_array + mem4 + 后端 + libtcc1
# + crt_crt1 (入口) + runmain (-run) → bin/lib 只需这一个文件
cp "$BASE/lib/libc-win.a"     "$BIN/lib/libc.a"

echo "=== [3/3] 部署编译器 (无 MUSL 形态: CONFIG_TCC_POSIX 默认链 musl libc.a) ==="
# 编译选项已逐一验证 (见 docs/simd-standard.md §9 与 build.sh 注释), 最终必要集:
#   CONFIG_TCC_POSIX=1 (tccpe.c:2089: 默认链 ELF libc.a 而非 msvcrt)
#   CONFIG_TCC_PREDEFS=1 (tccpp.c:3661: 编译期嵌入 tccdefs_.h, 自足)
# 已剔除 (实验验证不必要/有害):
#   CONFIG_TCC_MUSL=1 + 配套 (SEMLOCK=0/TCCDIR/MUSL_STDIO) — musl 线路的 tccrun 内存
#     模式在 psxscl mmap 强制 ≥1GB reserve 下映射落 2GB 上沿, R_X86_64_32S 重定位溢出
#     (tcc -run 报 relocation out of range); psxscl 无 clone/posix_spawn 实现, 临时 exe
#     fallback 亦不可用。无 MUSL 形态 -run/链接/测试全部正常。
#   ONE_SOURCE=1 (tcc.c 默认已是 1)
# bin/tcc.exe 即 [1/3] 产物 (运行时 GetModuleFileNameA 自动发现 bin/lib + bin/include)。
cp "$BASE/build/tcc-win.exe" "$BIN/tcc.exe"
echo "✓ 编译器就绪: $BIN/tcc.exe"

mkdir -p "$BASE/build/lib"
cp "$BASE/lib/libc-win.a" "$BASE/build/lib/libc.a"

echo "=== [4/4] 验证开箱即用 ==="
cd /tmp && rm -f bin_hello.c bin_hello.exe
cat > bin_hello.c <<'EOF'
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
int main(void) {
    printf("tcc_posix installed: %s\n", "ok");
    DIR *d = opendir(".");
    if (d) { printf("opendir ok\n"); closedir(d); }
    return 0;
}
EOF
"$BIN/tcc.exe" bin_hello.c -o bin_hello.exe 2>&1 | head -5
if [ -f bin_hello.exe ]; then
    ./bin_hello.exe && echo "✔ bin/tcc.exe 开箱即用"
else
    echo "✘ 链接失败"
fi
echo
echo "安装完成: $BIN"
du -sh "$BIN"
