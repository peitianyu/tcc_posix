#!/bin/bash
# install.sh - 生成 tcc_posix Windows 编译链安装 (bin/ 自足目录)
#
# 产物: bin/tcc.exe (开箱即用: tcc hello.c → hello.exe, 默认链 musl + psxscl)
#       bin/include/          musl 头 (nt64)
#       bin/lib/crt_crt1.o    入口对象 (唯一需显式的运行时对象)
#       bin/lib/libc.a        3.3M, 全部运行时: musl+chkstk+init_array+mem4
#                             +libtcc1+psxscl/ntapi/pemagine/dalist
#                             (零 Windows API 依赖 → 无 kernel32.def)
#       bin/lib/runmain.o     -run 入口 (_runmain), 纯计算程序可用
set -u
BASE="$(cd "$(dirname "$0")" && pwd)"
BOOT_TCC=tcc.exe

echo "=== [1/3] 自举 tcc-win (CONFIG_TCC_POSIX: 默认链 musl) ==="
"$BOOT_TCC" -DCONFIG_TCC_POSIX=1 -o "$BASE/build/tcc-win.exe" \
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

# 运行时对象 + 库 → lib/ (tcc 的 library_paths 含 <tccdir>/lib)
# crt_crt1.o 显式 (入口 _start); libc.a 含一切 (musl+chkstk+init_array+mem4
# +libtcc1+全部后端, 零 Windows API 依赖 → 无需 kernel32.def)
OBJ="$BASE/build/win-musl-obj"
cp "$OBJ/crt_crt1.c.o"        "$BIN/lib/"
cp "$BASE/lib/libc-win.a"     "$BIN/lib/libc.a"
# runmain.o: -run 入口 (_runmain), 纯计算程序可用 (完整 musl 初始化需 PE 启动)
"$BIN/tcc.exe" -c "$BASE/lib/runmain.c" -o "$BIN/lib/runmain.o" \
	-I "$BASE/src/posix/musl-nt64/include" -I "$BASE/src/posix/musl-nt64/arch/nt64" 2>/dev/null

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
