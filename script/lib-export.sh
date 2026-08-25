#!/bin/bash
# lib-export.sh - emit-c 产物独立库导出验收 (正式产物符号可见性, docs/desugar.md §8)
#
# 场景: 混合 model/operator/reflect/defer 的库模块 (无 main) 脱糖 → clang 编库 →
#       消费端链接调用。断言:
#   1) -Wall -Werror 门禁 (合成机制无告警);
#   2) 导出符号集 = 用户公开 API + operator 定义; 合成机制 (model 实例函数/
#      反射表) 全部 static 不导出;
#   3) -flto 编译 + 消费端直链运行 PASS (数字一致)。
# 用法: bash script/lib-export.sh    (需 WSL + clang)
set -u
BASE="$(cd "$(dirname "$0")/.." && pwd)"
TCC="$BASE/bin/tcc.exe"
OUT="$BASE/build/desugar"
mkdir -p "$OUT"
PASS=0; FAIL=0; FAILED=""

[ -x "$TCC" ] || { echo "错误: $TCC 不存在"; exit 1; }

SRC="$BASE/tests/libdemo_src.c"
CONS="$BASE/tests/libdemo_consumer.c"
DESC="$OUT/libdemo.desug.c"
OBJ="$OUT/libdemo.o"
EXE="$OUT/libdemo_consumer"

# 1) 脱糖 (无 main 库: 反射表须在全部 struct 定义后 + 前向声明, 2026-08-25 修)
if ! "$TCC" --emit-c -I "$BASE/lib" -I "$BASE" -o "$DESC" "$SRC" 2>"$OUT/libdemo.emit.err"; then
    echo "FAIL: emit-c 失败"; head -5 "$OUT/libdemo.emit.err"; exit 1
fi

# 2) WSL clang -flto -fvisibility=hidden -Wall -Werror 编库
W() {
    local tmp err rc
    tmp=$(mktemp); err=$(mktemp)
    MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' wsl -e bash -lc "$1" >"$tmp" 2>"$err"
    rc=$?
    if [ $rc -ne 0 ]; then tr -d '\000' <"$err" | grep -av "localhost\|无法\|wsl:" | grep -av '^$' >&2; fi
    tr -d '\000' <"$tmp" | grep -av "localhost\|无法\|wsl:" | grep -av '^$'
    rm -f "$tmp" "$err"
    return $rc
}
winc="/mnt/d/work/tcc_posix/include"
wsrc=$(echo "$DESC" | sed -E 's|^/([a-z])/|\1/|; s|^|/mnt/|')
wobj="/tmp/libdemo_export.o"
wexe=$(echo "$EXE" | sed -E 's|^/([a-z])/|\1/|; s|^|/mnt/|')
wcons=$(echo "$CONS" | sed -E 's|^/([a-z])/|\1/|; s|^|/mnt/|')

if ! W "clang -c -O2 -flto -fvisibility=hidden -Wall -Werror -I $winc $wsrc -o $wobj 2>&1"; then
    FAIL=$((FAIL+1)); FAILED="$FAILED 编库"; echo "FAIL: clang 编库 (-Wall -Werror / -flto)"; exit 1
fi
PASS=$((PASS+1)); echo "PASS 编库 (clang -O2 -flto -fvisibility=hidden -Wall -Werror)"

# 3) 导出符号集断言 (非 LTO 对象): 合成机制不得导出
if ! W "clang -c -O2 -fvisibility=hidden -I $winc $wsrc -o /tmp/libdemo_plain.o 2>&1"; then
    FAIL=$((FAIL+1)); FAILED="$FAILED 编库(plain)"; echo "FAIL: clang 编库 (plain)"; exit 1
fi
GLOBALS=$(W "nm -g /tmp/libdemo_plain.o | grep -a ' [TDBR] ' | awk '{print \$3}' | sort | tr '\\n' ' '")
EXPECTED="box_new_int box_val deferred_work operator_add_Pt operator_lt_Pt pt_fields vec_add vec_cmp"
GLOBALS="${GLOBALS% }"
if [ "$GLOBALS" = "$EXPECTED" ]; then
    PASS=$((PASS+1)); echo "PASS 导出符号集 = 用户 API + operator (合成机制全 static)"
else
    FAIL=$((FAIL+1)); FAILED="$FAILED 导出符号集"
    echo "FAIL: 导出符号集不符"
    echo "  expect: $EXPECTED"
    echo "  actual: $GLOBALS"
fi
if echo "$GLOBALS" | grep -q "box_set_int\|Pt_f\|Pt_refl"; then
    FAIL=$((FAIL+1)); FAILED="$FAILED 合成机制泄漏"
    echo "FAIL: 合成机制符号泄漏 (box_set_int/Pt_f/Pt_refl 应为 static)"
fi

# 4) 消费端: -flto 直链 + 运行
if W "clang -O2 -flto $wcons $wobj -o $wexe 2>&1 && $wexe"; then
    PASS=$((PASS+1)); echo "PASS 消费端链接+运行 (-flto, 数字一致)"
else
    FAIL=$((FAIL+1)); FAILED="$FAILED 消费端"; echo "FAIL: 消费端链接/运行"
fi

echo "=== lib-export: $PASS 通过, $FAIL 失败 ==="
[ -n "$FAILED" ] && echo "失败项:$FAILED" && exit 1
exit 0
