#!/bin/bash
# test.sh - tcc_posix 编译链测试套件
# 用法: ./test.sh              # Windows 编译+运行
#       ./test.sh -run         # 追加 tcc -run 模式
#       ./test.sh -linux       # 追加 Linux (WSL) 测试
#       ./test.sh -clean       # 清理测试产物
#
# 测试源码: tests/tNNN_*.c, 每个程序退出码 0 = 通过
set -u
BASE="$(cd "$(dirname "$0")" && pwd)"
cd "$BASE"   # listfile 相对路径与 tests/*.c 均基于仓库根
TCC="$BASE/bin/tcc.exe"
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

# 单测: 编译 → 链接 → 独立目录运行 (检查退出码)
run_win() {
    local name="$1"
    local src="tests/$name.c"
    [ -f "$src" ] || { echo "SKIP $name (无源码)"; return; }
    local o="$TESTDIR/$name.o" exe="$TESTDIR/$name.exe"
    # 头文件优先级: nt64 头 > include(Linux 兜底) > lib > 仓库根。nt64 库(链接对象)
    # 用 nt64 的 ucontext_t/mcontext_t 等 ABI, 若 include/ 先命中会拿到 Linux
    # 布局导致 ucontext 协程(test t060/t061)结构体错位 → swapcontext 段错误。
    # 仓库根(-I "$BASE")让 STL 测试 (t062/t063) 的 #include "lib/stl/..." 命中
    # lib/stl/.h, 而非被 $BASE/lib 误解析成 lib/lib/stl/...。
    if ! "$TCC" -c "$src" -o "$o" @build/tests-common.list \
        2>"$TESTDIR/$name.cerr"; then
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(编译)"; echo "FAIL $name: 编译错误"; head -3 "$TESTDIR/$name.cerr"; return
    fi
    # 个别测试需要额外源文件一并链接 (如 t049_cpu 依赖 lib/cpu-prof.c)
    local extra=""
    case "$name" in
        t049_cpu) extra="$BASE/lib/cpu-prof.c" ;;
    esac
    if ! "$TCC" "$o" $extra -I "$BASE/lib" -o "$exe" 2>"$TESTDIR/$name.lerr"; then
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
    # -run 内存执行局限 (独立 exe / -linux 下均已覆盖):
    #   t041: dladdr 无模块加载概念 → 返回 0
    #   t049: 单文件模式不含 extra 源 lib/cpu-prof.c (cpu_prof_* 未定义)
    case "$name" in
        t041_dlfcn) echo "SKIP $name (-run: dladdr 无模块)"; return ;;
        t049_cpu)   echo "SKIP $name (-run: 需 extra 源 cpu-prof.c)"; return ;;
    esac
    local out rc
    out=$(cd "$BASE" && timeout 15 "$TCC" -run @build/tests-common.list "$src" 2>&1); rc=$?
    if [ "$rc" = "0" ]; then
        PASS=$((PASS+1)); echo "PASS $name (-run)"
    else
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(-run rc=$rc)"; echo "FAIL $name (-run): rc=$rc"
        echo "$out" | head -5
    fi
}

# Linux (WSL) 编译运行
run_linux() {
    local name="$1"
    local src="tests/$name.c"
    [ -f "$src" ] || return
    local lnx="$BASE/build/tcc-linux.exe"
    [ -x "$lnx" ] || { echo "SKIP $name (无 tcc-linux.exe)"; return; }
    local o="$TESTDIR/${name}_lnx.o" exe="$TESTDIR/${name}_linux"
    if ! "$lnx" -c "$src" -o "$o" @build/tests-common.list \
        2>"$TESTDIR/$name.lcerr"; then
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(lnx编译)"; echo "FAIL $name (lnx编译)"; head -3 "$TESTDIR/$name.lcerr"; return
    fi
    # 个别测试需要额外源文件一并链接 (如 t049_cpu 依赖 lib/cpu-prof.c)
    local extra_obj=""
    case "$name" in
        t049_cpu)
            extra_obj="$TESTDIR/${name}_extra.o"
            if ! "$lnx" -c "$BASE/lib/cpu-prof.c" -o "$extra_obj" @build/tests-common.list \
                2>"$TESTDIR/$name.lcerr"; then
                FAIL=$((FAIL+1)); FAILED="$FAILED $name(lnx编译-extra)"; echo "FAIL $name (lnx编译-extra)"; head -3 "$TESTDIR/$name.lcerr"; return
            fi
            ;;
    esac
    if ! ( cd "$BASE/build/linux-musl-obj" && "$lnx" -nostdlib -static \
        asm_crt1.o "$o" init_array.o init_fini.o \
        libtcc1.o va_list.o ${extra_obj:+$extra_obj} libc.a -o "$exe" ) 2>"$TESTDIR/$name.llerr"; then
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(lnx链接)"; echo "FAIL $name (lnx链接)"; head -3 "$TESTDIR/$name.llerr"; return
    fi
    # WSL 运行: D:/work/tcc_posix/build/tests/x_linux → /mnt/d/work/tcc_posix/build/tests/x_linux
    local wslpath
    wslpath=$(echo "$exe" | sed -E 's|^/([a-z])/|\1/|; s|^|/mnt/|')
    local out rc
    out=$(MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' timeout 15 wsl -e bash -lc "'$wslpath'" 2>&1); rc=$?
    if [ "$rc" = "0" ]; then
        PASS=$((PASS+1)); echo "PASS $name (linux)"
    else
        FAIL=$((FAIL+1)); FAILED="$FAILED $name(lnx rc=$rc)"; echo "FAIL $name (linux): rc=$rc"
        echo "$out" | head -5
    fi
}

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
