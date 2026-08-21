#!/bin/bash
# build_musl.sh - compile musl-1.1.11 + mmglue arch overrides with TinyCC
# Output: musl_build/obj/*.o -> later archived as libc.a
set -u
BASE="$(cd "$(dirname "$0")/.." && pwd)"
TCC="$BASE/build/tcc-win.exe"
MB="$BASE/src/posix"
MUSL="$MB/musl-nt64"
MM="$MB/musl-nt64"
OUT="$BASE/build/win-musl-obj"
mkdir -p "$OUT"

CFLAGS="-c -std=c99 -ffreestanding -nostdinc -D_XOPEN_SOURCE=700 -fomit-frame-pointer -Os \
	-I $MM/src/internal -I $MM/arch/nt64 -I $MM/arch/x86_64 -I $MUSL/include"

ok=0; fail=0; failed=""
# tcc-win64 workaround: fmt_u 无局部变量版 (tccgen 局部初始化 bug, printf %d 崩溃)
grep -q "no local vars" "$MUSL/src/stdio/vfprintf.c" || python - <<'PYEOF'
import re
fp = r'%s/src/stdio/vfprintf.c' % r'$MUSL'
ft = open(fp, encoding='utf-8').read()
old = "static char *fmt_u(uintmax_t x, char *s)
{
	unsigned long y;
	for (   ; x>ULONG_MAX; x/=10) *--s = '0' + x%10;
	for (y=x;           y; y/=10) *--s = '0' + y%10;
	return s;
}"
new = "static char *fmt_u(uintmax_t x, char *s)
{
	/* tcc-win64 workaround: no local vars */
	while (x > 9) {
		*--s = '0' + x % 10;
		x /= 10;
	}
	*--s = '0' + x;
	return s;
}"
if old in ft:
    open(fp, 'w', encoding='utf-8').write(ft.replace(old, new))
    print('fmt_u patch applied')
PYEOF

# musl src C files
# (排除: complex/math/ldso 已从源码目录删除; 架构子目录 x86_64/i386/nt32 已删)
# 注: thread/ 根的 musl 原版不直接编译 (由 nt64 薄包装 include, 见 src/thread/nt64/pthread_*.c);
#     env/__init_tls.c 提供 __copy_tls (pthread_create 需要), 必须编译
# 并行 + 增量: xargs -P 多进程, 每进程批量处理 (Windows 进程启动开销大)
mkobj() {
	local f rel obj err
	for f in "$@"; do
		rel="${f#$MUSL/src/}"
		obj="$OUT/musl_${rel//\//_}.o"
		err="$OUT/.err.${obj##*/}"
		[ -f "$obj" ] && [ "$obj" -nt "$f" ] && continue
		if "$TCC" $CFLAGS "$f" -o "$obj" 2>"$err" ; then
			echo "OK: $rel"
		else
			echo "FAIL: $rel"
			head -3 "$err" | sed 's/^/    /' >&2
		fi
	done
}
export -f mkobj
export TCC CFLAGS MUSL OUT
find "$MUSL/src" -name '*.c' \
	! -path '*/thread/__set_thread_area.c' ! -path '*/thread/pthread_detach.c' \
	! -path '*/thread/pthread_equal.c' ! -path '*/thread/pthread_self.c' \
	! -name '*res_*' ! -name 'lookup_*' | sort | \
	xargs -P "${JOBS:-8}" -n "${BATCH:-25}" bash -c 'mkobj "$@"' _ 2>/dev/null | \
	awk '/^OK:/{ok++} /^FAIL:/{fail++; f=f" "$2} END{print "C: "ok" ok, "fail" fail" f}'

# nt64 架构汇编: setjmp/longjmp (src/setjmp/{setjmp,longjmp}.c 是 0 字节占位,
# 实现全在 arch 汇编) + syscall_cp (可取消系统调用底层, mq/timer/aio 需要);
# 其余 arch 汇编符号已由 C 实现提供, 不重复编译避免重定义)
for f in "$MUSL/src/setjmp/nt64/setjmp.s" "$MUSL/src/setjmp/nt64/longjmp.s" \
	"$MUSL/src/thread/nt64/syscall_cp.s"; do
	rel="${f#$MUSL/src/}"
	obj="$OUT/musl_${rel//\//_}.o"
	if "$TCC" $CFLAGS "$f" -o "$obj" 2>"$OUT/.err.${obj##*/}"; then
		echo "OK: $rel"
	else
		echo "FAIL: $rel"; head -3 "$OUT/.err.${obj##*/}" >&2
	fi
done

# tcc-win64 大栈帧 chkstk (真实现, 替代 TCC 空桩)
"$TCC" -c "$MM/arch/nt64/src/chkstk.s" -o "$OUT/chkstk.o" 2>/dev/null || true

# mmglue arch/nt64/src (并行 + 增量, 批量)
mkmmg() {
	local f rel obj err
	for f in "$@"; do
		rel="${f#$MM/arch/nt64/src/}"
		obj="$OUT/mmg_${rel//\//_}.o"
		err="$OUT/.err.${obj##*/}"
		[ -f "$obj" ] && [ "$obj" -nt "$f" ] && continue
		if "$TCC" $CFLAGS "$f" -o "$obj" 2>"$err" ; then
			echo "OK: mmg/$rel"
		else
			echo "FAIL: mmg/$rel"
			head -3 "$err" | sed 's/^/    /' >&2
		fi
	done
}
export -f mkmmg
export MM
find "$MM/arch/nt64/src" -name '*.c' | sort | \
	xargs -P "${JOBS:-8}" -n "${BATCH:-25}" bash -c 'mkmmg "$@"' _ 2>/dev/null | \
	awk '/^OK:/{ok++} /^FAIL:/{fail++; f=f" "$2} END{print "mmg: "ok" ok, "fail" fail" f}'
# (mmglue src/ overrides are merged into the tree)

# --- crt 对象 (build_both.sh 链接模板需要; 不在 src/ 下, 单独编译) ---
#   crt_crt1.c.o crt_Scrt1.c.o crt_crtdev.c.o crt_crtposix.c.o + crti/crtn/init_array 汇编
#   crtdev.c/crtposix.c 需要 psxglue.h (arch/nt64/src) 与 __PSXOPT_* 定义
CRT_CFLAGS="$CFLAGS -I $MM/arch/nt64/src"
for f in crt1.c Scrt1.c crtdev.c crtposix.c; do
	if "$TCC" $CRT_CFLAGS "$MM/crt/$f" -o "$OUT/crt_$f.o" 2>"$OUT/.err"; then
		ok=$((ok+1))
	else
		fail=$((fail+1)); failed="$failed crt/$f"
		echo "FAIL: crt/$f"
		head -2 "$OUT/.err" | sed 's/^/    /'
	fi
done
for f in crti crtn init_array; do
	if "$TCC" $CFLAGS "$MM/crt/$f.s" -o "$OUT/$f.o" 2>"$OUT/.err"; then
		ok=$((ok+1))
	else
		fail=$((fail+1)); failed="$failed crt/$f.s"
		echo "FAIL: crt/$f.s"
		head -2 "$OUT/.err" | sed 's/^/    /'
	fi
done
# frexpl: math/ 目录已排除编译, 但链接需要 (printf %Lf 等) — 单独编译进 libc.a
# frexp: frexpl 的 53 位分支 (LDBL_MANT_DIG==53) 委托给 double 版 frexp
for mf in frexpl frexp; do
if "$TCC" $CFLAGS "$MM/src/math/$mf.c" -o "$OUT/$mf.o" 2>"$OUT/.err"; then
	ok=$((ok+1))
else
	fail=$((fail+1)); failed="$failed math/$mf.c"
	echo "FAIL: math/$mf.c"
	head -2 "$OUT/.err" | sed 's/^/    /'
fi
done

# --- libc.a 打包 (mingw ar @objlist.txt; 命令行 >32K 会被截断) ---
#   排除: hello 测试 / crt_* (入口 _start 需显式提供, 防 libtcc1 内 crt1.o 冲突)
#   chkstk.o 与 init_array.o 并入库内 (闭包提取可自动找到)
if [ -x /c/msys64/mingw64/bin/ar ]; then
	( cd "$OUT" && for f in *.o; do case "$f" in
		hello*|libc.a|crt_Scrt1*|crt_crtdev*|crt_crtposix*) ;;
		*) echo "$f" ;;
	esac; done > objlist.txt && /c/msys64/mingw64/bin/ar rcs libc.a @objlist.txt && rm -f objlist.txt )
	echo "libc.a: $(ls -la "$OUT/libc.a" 2>/dev/null | awk '{print $5}') bytes ($(/c/msys64/mingw64/bin/ar t "$OUT/libc.a" 2>/dev/null | wc -l) 成员)"
else
	echo "WARN: mingw ar 未找到, 跳过 libc.a 打包 (手动执行: ar rcs $OUT/libc.a ...)"
fi
rm -f "$OUT/.err"

# --- 后端 (psxscl/ntapi/pemagine/dalist) 并入 libc.a ---
# 展开各后端库的 .o (避免嵌套归档) 追加进 libc.a; 依赖各库先构建 (build.sh 顺序)
if [ -x /c/msys64/mingw64/bin/ar ]; then
	BK_DIR="$OUT/.backend"
	rm -rf "$BK_DIR" && mkdir -p "$BK_DIR"
	for bklib in libpsxscl.a libntapi.a libpemagine.a libdalist.a; do
		for bdir in psxscl-2015 ntapi pemagine dalist; do
			BKSRC="$BASE/build/$bdir/$bklib"
			[ -f "$BKSRC" ] && break
		done
		[ -f "$BKSRC" ] || continue
		( cd "$BK_DIR" && /c/msys64/mingw64/bin/ar x "$BKSRC" )
	done
	( cd "$BK_DIR" && ls *.o >/dev/null 2>&1 && /c/msys64/mingw64/bin/ar r "$OUT/libc.a" *.o )
	rm -rf "$BK_DIR"
	echo "libc.a (含后端): $(/c/msys64/mingw64/bin/ar t "$OUT/libc.a" 2>/dev/null | wc -l) 成员"
fi

# --- libtcc1 (TCC 运行时) 并入 libc.a ---
# -run 在 musl 链下不可用, 故 libtcc1 无需独立; 完整并入 (含 crt1.o 亦无冲突,
# 因 crt_crt1.o 先加载定义 _start, 闭包提取不会碰 crt1.o)。
# 随后剔除 msvcrt 时代 CRT 成员 (crt1/crt1w/wincrt1/wincrt1w/tcov/dllcrt1/
# dllmain/winex): 它们引用 kernel32 API, 而 musl/psxscl 零 Windows 依赖 →
# 剔除后 libc.a 完全自足, 无需 kernel32.def
if [ -x /c/msys64/mingw64/bin/ar ]; then
	BK_DIR="$OUT/.libtcc1"
	rm -rf "$BK_DIR" && mkdir -p "$BK_DIR"
	( cd "$BK_DIR" && /c/msys64/mingw64/bin/ar x "$BASE/lib/libtcc1.a" && \
		/c/msys64/mingw64/bin/ar r "$OUT/libc.a" *.o )
	rm -rf "$BK_DIR"
	for m in crt1.o crt1w.o wincrt1.o wincrt1w.o tcov.o dllcrt1.o dllmain.o winex.o; do
		/c/msys64/mingw64/bin/ar d "$OUT/libc.a" "$m" 2>/dev/null
	done
fi

# --- runmain.o (-run 入口) 编译并入 libc.a ---
# tccrun.c: runmain.o 文件缺失时 fallback: set_global_sym(_runmain) + 重载
# libc.a → alacarte 提取 _runmain 成员
if [ -x /c/msys64/mingw64/bin/ar ]; then
	"$TCC" -c "$BASE/lib/runmain.c" -o "$OUT/runmain.o" \
		-I "$MM/include" -I "$MM/arch/nt64" 2>/dev/null
	/c/msys64/mingw64/bin/ar r "$OUT/libc.a" "$OUT/runmain.o" 2>/dev/null
	rm -f "$OUT/runmain.o"
	echo "libc.a (含 runmain): $(/c/msys64/mingw64/bin/ar t "$OUT/libc.a" 2>/dev/null | wc -l) 成员"
fi
echo "=============================="
echo "musl compiled OK: $ok   FAILED: $fail"
[ -n "$failed" ] && echo "failed:$failed"
exit $fail
