#!/bin/bash
# build.sh - tcc_posix 完整编译链一键构建
#
# 产物:
#   build/tcc-win.exe     Windows 目标 TCC (PE)
#   build/tcc-linux.exe   Linux 目标 TCC (ELF)
#   build/win-musl-obj/   Windows musl libc.a + crt + 后端
#   build/linux-musl-obj/ Linux musl libc.a + crt
#   lib/*.a               固化库 (libc-win/libc-linux/psxscl/ntapi/pemagine/dalist)
#   include/              固化 musl 头 (Linux)
set -u
BASE="$(cd "$(dirname "$0")" && pwd)"
BOOT_TCC=/d/work/tinycc/win32/tcc.exe   # 自举用宿主 TCC (仅第一次需要)

echo "=== [1/4] 自举 TCC (Windows + Linux 目标) ==="
[ -f "$BASE/build/tcc-win.exe" ] || "$BOOT_TCC" -o "$BASE/build/tcc-win.exe" \
	"$BASE/src/tcc.c" -I"$BASE/src" -DONE_SOURCE=1
[ -f "$BASE/build/tcc-linux.exe" ] || "$BOOT_TCC" -DTCC_TARGET_X86_64 -DCONFIG_TCC_PREDEFS=1 \
	-o "$BASE/build/tcc-linux.exe" "$BASE/src/tcc.c" -I"$BASE/src" -DONE_SOURCE=1
"$BASE/build/tcc-win.exe" -v && "$BASE/build/tcc-linux.exe" -v
# 宿主 TCC 运行时 (并入 libc.a 用; 后续可自产)
mkdir -p "$BASE/lib"
[ -f "$BASE/lib/libtcc1.a" ] || cp "$(dirname "$BOOT_TCC")/lib/libtcc1.a" "$BASE/lib/"

echo "=== [2/4] Windows 后端 (psxscl/ntapi/pemagine/dalist) ==="
bash "$BASE/script/build_psxscl.sh" && bash "$BASE/script/build_ntapi.sh" \
	&& bash "$BASE/script/build_pemagine.sh" && bash "$BASE/script/build_dalist.sh"

echo "=== [3/4] musl libc (Windows + Linux) ==="
bash "$BASE/script/build_musl.sh" && bash "$BASE/script/build_musl_linux.sh"

echo "=== [4/4] 固化 include/ 与 lib/ ==="
# Linux musl 头
rm -rf "$BASE/include" && mkdir -p "$BASE/include"
cp -r "$BASE/src/posix/musl-1.1.11/include/"* "$BASE/include/"
cp -r "$BASE/build/linux-musl-inc/bits" "$BASE/include/"
cp "$BASE/build/linux-musl-inc/version.h" "$BASE/include/"
# 库
cp "$BASE/build/win-musl-obj/libc.a"      "$BASE/lib/libc-win.a"
cp "$BASE/build/linux-musl-obj/libc.a"    "$BASE/lib/libc-linux.a"
cp "$BASE/build/psxscl-2015/libpsxscl.a"  "$BASE/lib/"
cp "$BASE/build/ntapi/libntapi.a"         "$BASE/lib/"
cp "$BASE/build/pemagine/libpemagine.a"   "$BASE/lib/"
cp "$BASE/build/dalist/libdalist.a"       "$BASE/lib/"

echo "=== 完成 ==="
ls -la "$BASE/lib/"*.a | awk '{print "  ", $5, $9}'
echo "日常编译: build/tcc-win.exe -platform=win hello.c    (→ hello.exe)"
echo "          build/tcc-win.exe -platform=linux hello.c  (→ hello_linux)"
