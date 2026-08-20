#!/bin/bash
# test.sh - tcc_posix 编译链测试套件
# 用法: ./test.sh              # 全部测试 (Windows 编译+运行)
#       ./test.sh -linux       # 追加 Linux 测试 (需 WSL + tcc-linux.exe)
#       ./test.sh -run         # 追加 tcc -run 模式测试
#       ./test.sh -clean       # 清理测试产物
#
# 测试源码: tests/tNNN_*.c, 每个程序退出码 0 = 通过
set -u
BASE="$(cd "$(dirname "$0")" && pwd)"
TCC="$BASE/bin/tcc.exe"
INC_WIN="$BASE/src/posix/musl-nt64/include"
ARCH_WIN="$BASE/src/posix/musl-nt64/arch/nt64"
TESTDIR="$BASE/build/tests"
PASS=0; FAIL=0; FAILED=""
MODE_LINUX=0; MODE_RUN=0

for a in "$@"; do
    case "$a" in
        -linux) MODE_LINUX=1 ;;
        -run) MODE_RUN=1 ;;
        -clean) rm -rf "$TESTDIR"; echo "已清理 $TESTDIR"; exit 0 ;;
        *) echo "未知选项: $a"; exit 1 ;;
    esac
done

mkdir -p "$TESTDIR"
[ -x "$TCC" ] || { echo "错误: $TCC 不存在 (先运行 ./install.sh)"; exit 1; }

echo "=== tcc_posix 测试套件 ==="
echo "编译器: $("$TCC" -v 2>&1 | head -1)"

# 单测: 复制源码 → 编译 → 链接 → 运行 (检查退出码)
# run_win <name>
run_win() {
    local name="$1"
    local src="tests/$name.c"
    [ -f "$src" ] || { echo "SKIP $name (无源码)"; return; }
    local o="$TESTDIR/$name.o" exe="$TESTDIR/$name.exe"
    if ! "$TCC" -c "$src" -o "$o" -I "$INC_WIN" -I "$ARCH_WIN" \
        -std=c99 -D_XOPEN_SOURCE=700 2>"$TESTDIR/$name.cerr"; then
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(编译)"; echo "FAIL $name: 编译错误"; head -3 "$TESTDIR/$name.cerr"; return
    fi
    if ! "$TCC" "$o" -o "$exe" 2>"$TESTDIR/$name.lerr"; then
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(链接)"; echo "FAIL $name: 链接错误"; head -3 "$TESTDIR/$name.lerr"; return
    fi
    local out rc
    local tdir="$TESTDIR/run_$name"
    mkdir -p "$tdir"
    out=$(cd "$tdir" && "../$name.exe" 2>&1); rc=$?
    if [ "$rc" = "0" ]; then
        PASS=$((PASS+1)); echo "PASS $name (win)"
    else
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(rc=$rc)"; echo "FAIL $name: rc=$rc"
        echo "$out" | head -5
    fi
}

# tcc -run 模式 (内存执行)
run_run() {
    local name="$1"
    local src="tests/$name.c"
    [ -f "$src" ] || return
    local out rc
    out=$(cd "$BASE" && timeout 15 "$TCC" -run -I "$INC_WIN" -I "$ARCH_WIN" "$src" 2>&1); rc=$?
    if [ "$rc" = "0" ]; then
        PASS=$((PASS+1)); echo "PASS $name (-run)"
    else
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(-run rc=$rc)"; echo "FAIL $name (-run): rc=$rc"
        echo "$out" | head -5
    fi
}

# Linux (WSL) 编译运行
run_linux() {
    local name="$1" src="tests/$name.c"
    [ -f "$src" ] || return
    local lnx="$BASE/build/tcc-linux.exe"
    [ -x "$lnx" ] || { echo "SKIP $name (无 tcc-linux.exe)"; return; }
    local o="$TESTDIR/${name}_lnx.o" exe="$TESTDIR/${name}_linux"
    if ! "$lnx" -c "$src" -o "$o" -I "$BASE/include" -I "$ARCH_LNX" \
        -std=c99 -D_XOPEN_SOURCE=700 2>"$TESTDIR/$name.lcerr"; then
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(lnx编译)"; echo "FAIL $name (lnx编译)"; head -3 "$TESTDIR/$name.lcerr"; return
    fi
    if ! ( cd "$BASE/build/linux-musl-obj" && "$lnx" -nostdlib -static \
        asm_crt1.o "$o" init_array.o init_fini.o \
        libtcc1.o va_list.o libc.a -o "$exe" ) 2>"$TESTDIR/$name.llerr"; then
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(lnx链接)"; echo "FAIL $name (lnx链接)"; head -3 "$TESTDIR/$name.llerr"; return
    fi
    local out rc
    out=$(MSYS_NO_PATHCONV=1 timeout 15 wsl -e "/mnt/$(echo "${exe#/}" | tr ':' '/' | tr '\\\\' '/' | sed 's|^\([a-z]\)|\\1|')" 2>&1); rc=$?
    # 简化: 用相对路径复制到可访问处
    out=$(cp "$exe" /tmp/ 2>/dev/null && MSYS_NO_PATHCONV=1 timeout 15 wsl -e "/mnt/c/Users/12165/AppData/Local/Temp/$(basename "$exe")" 2>&1); rc=$?
    if [ "$rc" = "0" ]; then
        PASS=$((PASS+1)); echo "PASS $name (linux)"
    else
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(lnx rc=$rc)"; echo "FAIL $name (linux): rc=$rc"
        echo "$out" | head -5
    fi
    rm -f /tmp/$(basename "$exe")
}

ARCH_LNX="$BASE/src/posix/musl-1.1.11/arch/x86_64"

echo "--- Windows 编译+运行 ---"
for src in tests/t*.c; do
    run_win "$(basename "$src" .c)"
done

if [ "$MODE_RUN" = "1" ]; then
    echo "--- tcc -run 模式 ---"
    for src in tests/t*.c; do
        run_run "$(basename "$src" .c)"
    done
fi

if [ "$MODE_LINUX" = "1" ]; then
    echo "--- Linux (WSL) ---"
    for src in tests/t*.c; do
        run_linux "$(basename "$src" .c)"
    done
fi

echo ""
echo "=== 结果: $PASS 通过, $FAIL 失败 ==="
[ -n "$FAILED" ] && echo "失败项:$FAILED" && exit 1
exit 0
