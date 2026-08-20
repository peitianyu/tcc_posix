#!/bin/bash
# build_pemagine.sh - compile pemagine lib with TinyCC
set -u
BASE="$(cd "$(dirname "$0")/.." && pwd)"
TCC="$BASE/build/tcc-win.exe"
SRC="$BASE/src/posix/components/pemagine"
OUT="$BASE/build/$(basename $SRC)"
mkdir -p "$OUT"

CFLAGS="-c -DMIDIPIX_FREESTANDING -D__NT64 -D__GNUC__=4 -D__amd64=1 -D__SIZEOF_POINTER__=8 \
	-U_WIN32 -UWIN32 -U__WIN32__ -U_WIN64 -UWIN64 -U__WIN64__ \
	-ffreestanding -fno-builtin \
	-include $BASE/src/posix/components/midipix_tcc_compat.h \
	-I $BASE/src/posix/components/compat_include \
	-I $SRC/src/internal -I $SRC/include -I $BASE/src/posix/components/psxtypes/include -I $BASE/src/posix/components/dalist/include"

ok=0; fail=0; failed=""
for f in $(find "$SRC/src" -name '*.c' ! -name 'pe_entry_point.c' | sort); do
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
echo "pemagine compiled OK: $ok   FAILED: $fail"
[ -n "$failed" ] && echo "failed files:$failed"
# 打包 libpemagine.a
if [ -x /c/msys64/mingw64/bin/ar ]; then
	( cd "$OUT" && find . -name '*.o' -printf '%P\n' > objlist.txt && \
		/c/msys64/mingw64/bin/ar rcs libpemagine.a @objlist.txt && rm -f objlist.txt )
	echo "libpemagine.a: $(/c/msys64/mingw64/bin/ar t "$OUT/libpemagine.a" 2>/dev/null | wc -l) 成员"
fi
exit $fail
