#!/bin/bash
# build_musl_linux.sh - musl 1.1.11 Linux x86_64 版 (TCC 编译, 交叉)
# 编译器: tcc-linux-x86_64.exe (Linux 目标; 由 win32/tcc.exe 自举: 
#   cd /d/work/tinycc && ./win32/tcc.exe -o tcc-linux-x86_64.exe tcc.c -I. -DONE_SOURCE=1)
set -u
BASE="$(cd "$(dirname "$0")/.." && pwd)"
TCC="$BASE/build/tcc-linux.exe"
MUSL="$BASE/src/posix/musl-1.1.11"
OUT="$BASE/build/linux-musl-obj"
mkdir -p "$OUT" "$BASE/build/linux-musl-inc/bits"

# 1. alltypes.h + bits (musl 原生)
sed -f "$MUSL/tools/mkalltypes.sed" \
    "$MUSL/arch/x86_64/bits/alltypes.h.in" "$MUSL/include/alltypes.h.in" \
    > "$BASE/build/linux-musl-inc/bits/alltypes.h"
cp "$MUSL/arch/x86_64/bits/"*.h "$BASE/build/linux-musl-inc/bits/" 2>/dev/null
rm -f "$BASE/build/linux-musl-inc/bits/alltypes.h.in"
printf '#define VERSION "1.1.11-linux-tcc"\n' > "$BASE/build/linux-musl-inc/version.h"

CFLAGS="-c -std=c99 -ffreestanding -nostdinc -D_XOPEN_SOURCE=700 -fomit-frame-pointer -Os \
	-I $MUSL/src/internal -I $MUSL/arch/x86_64 \
	-I $BASE/build/linux-musl-inc -I $MUSL/include"

# 2. 编译 C (排除 complex/math 主块/ldso/DNS; math 辅助单独)
for f in $(find "$MUSL/src" -name "*.c" ! -path "*complex*" ! -path "*math*" \
        ! -path "*ldso*" ! -name "*res_*" ! -name "lookup_*" | sort); do
	rel="${f#$MUSL/src/}"; obj="$OUT/${rel//\//_}.o"
	[ -f "$obj" ] || "$TCC" $CFLAGS "$f" -o "$obj" || echo "FAIL: $rel"
done
for f in __signbitl __fpclassifyl frexpl; do
	obj="$OUT/math_$f.o"
	[ -f "$obj" ] || "$TCC" $CFLAGS "$MUSL/src/math/$f.c" -o "$obj" || echo "FAIL: math/$f"
done

# 3. 汇编 x86_64 .s (排除 Scrt1 共享 crt)
for f in $(find "$MUSL/src" "$MUSL/crt" -path "*x86_64*" -name "*.s" \
        ! -path "*math*" ! -path "*ldso*" ! -name "Scrt1.s" | sort); do
	obj="$OUT/asm_$(basename "$f" .s).o"
	[ -f "$obj" ] || "$TCC" -c "$f" -o "$obj" || echo "FAIL asm: $f"
done

# 4. 专用对象: init_array (空段), init_fini (ret 版 _init/_fini), libtcc1, va_list
[ -f "$OUT/init_array.o" ] || "$TCC" -c "$BASE/lib/crt/init_array.s" -o "$OUT/init_array.o"
[ -f "$OUT/init_fini.o" ]   || "$TCC" -c "$BASE/lib/crt/init_fini.s"   -o "$OUT/init_fini.o"
[ -f "$OUT/libtcc1.o" ]     || "$TCC" -c /d/work/tinycc/lib/libtcc1.c -o "$OUT/libtcc1.o"
[ -f "$OUT/va_list.o" ]     || "$TCC" -c /d/work/tinycc/lib/va_list.c -o "$OUT/va_list.o"

# 5. 打包 libc.a (mingw ar @objlist.txt; TCC -ar 命令行 >32K 会被截断, 且产物 mingw 工具读不了)
#   排除: 链接模板显式列的 obj; 以及 C 版 __set_thread_area/__unmapself
#   (调 SYS_set_thread_area(129), x86_64/WSL1 上 ENOSYS — 用汇编版 asm_*, 走 arch_prctl)
cd "$OUT" && rm -f libc.a objlist.txt
for f in *.o; do case "$f" in
	libtcc1*|va_list*|asm_crt1*|init_fini*|hello*|h2*|thread___set_thread_area*|thread___unmapself*) ;;
	*) echo "$f" ;;
esac; done > objlist.txt
/c/msys64/mingw64/bin/ar rcs libc.a @objlist.txt && rm -f objlist.txt
echo "libc.a: $(/c/msys64/mingw64/bin/ar t libc.a | wc -l) 成员"
echo "链接示例:"
echo "  $TCC -nostdlib -static asm_crt1.o hello.o init_array.o init_fini.o libc.a libtcc1.o va_list.o -o hello_linux"
