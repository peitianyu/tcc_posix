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
BOOT_TCC="${BOOT_TCC:-tcc.exe}"
"$BOOT_TCC" -DCONFIG_TCC_POSIX=1 -DCONFIG_TCC_PREDEFS=1 -o "$BASE/build/tcc-win.exe" \
    "$BASE/src/tcc.c" -I"$BASE/src" -DONE_SOURCE=1 2>&1 | grep -iE 'error' | head -5
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

echo "=== [3/3] 验证开箱即用 ==="
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
