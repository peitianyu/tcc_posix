#!/bin/bash
# build_ntapi.sh - compile ntapi (2015-era checkout) with TinyCC
set -u
BASE="$(cd "$(dirname "$0")/.." && pwd)"
TCC="$BASE/build/tcc-win.exe"
SRC="$BASE/src/posix/components/ntapi"
OUT="$BASE/build/$(basename $SRC)"
mkdir -p "$OUT"

CFLAGS="-c -DMIDIPIX_FREESTANDING -D__NT64 -DNTAPI_BUILD -D__GNUC__=4 -D__amd64=1 -D__SIZEOF_POINTER__=8 \
	-U_WIN32 -UWIN32 -U__WIN32__ -U_WIN64 -UWIN64 -U__WIN64__ \
	-ffreestanding -fno-builtin \
	-include $BASE/src/posix/components/midipix_tcc_compat.h \
	-I $BASE/src/posix/components/compat_include \
	-I $SRC/src/internal -I $SRC/include -I $BASE/src/posix/components/psxtypes/include -I $BASE/src/posix/components/pemagine/include -I $BASE/src/posix/components/dalist/include"

ok=0; fail=0; failed=""
for f in $(find "$SRC/src" -name '*.c' | sort); do
	rel="${f#$SRC/}"
	obj="$OUT/${rel%.c}.o"
	mkdir -p "$(dirname "$obj")"
	if "$TCC" $CFLAGS "$f" -o "$obj" 2>"$OUT/.err" ; then
		ok=$((ok+1))
	else
		fail=$((fail+1))
		failed="$failed $rel"
		echo "FAIL: $rel"
		head -3 "$OUT/.err" | sed 's/^/    /'
	fi
done
rm -f "$OUT/.err"
echo "=============================="
echo "ntapi compiled OK: $ok   FAILED: $fail"
[ -n "$failed" ] && echo "failed files:$failed"
# 汇编源 (GNU as 语法 .s; __tt_fork_v1 等定义在此, TCC 汇编器可编; nt32 是 32 位代码不编)
for f in $(find "$SRC/src" -name '*.s' ! -path '*/nt32/*' | sort); do
	rel="${f#$SRC/}"; obj="$OUT/${rel%.s}.o"
	mkdir -p "$(dirname "$obj")"
	if "$TCC" $CFLAGS "$f" -o "$obj" 2>"$OUT/.err"; then
		ok=$((ok+1))
	else
		fail=$((fail+1)); failed="$failed $rel"
		echo "FAIL: $rel"
		head -3 "$OUT/.err" | sed 's/^/    /'
	fi
done
# 打包 libntapi.a
# 打包 libntapi.a (mingw ar @objlist.txt)
# 重名修复: src/process/tt_fork_v1.c (C 版) 与 src/process/nt64/tt_fork_v1.s
# (定义 __tt_fork_v1) 同名 → 归档重名覆盖, nt64 版改名保留
if [ -x /c/msys64/mingw64/bin/ar ]; then
	( cd "$OUT" && find . -name '*.o' -printf '%P\n' > objlist.txt && \
		mv src/process/nt64/tt_fork_v1.o src/process/nt64/tt_fork_v1_nt64.o 2>/dev/null; \
		sed -i 's|src/process/nt64/tt_fork_v1.o|src/process/nt64/tt_fork_v1_nt64.o|' objlist.txt 2>/dev/null; \
		/c/msys64/mingw64/bin/ar rcs libntapi.a @objlist.txt && rm -f objlist.txt )
	echo "libntapi.a: $(/c/msys64/mingw64/bin/ar t "$OUT/libntapi.a" 2>/dev/null | wc -l) 成员"
fi
exit $fail
