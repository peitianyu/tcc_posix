# desugar.ps1 - desugared-C clang driver: numeric-consistency check
#   (host tcc does --emit-c / -run; WSL clang -O3 compiles+runs the desugar output)
#
# Pipeline:
#   1) host tcc --emit-c  <src>          -> <out>/<base>.desug.c  (keep #include, no inline)
#   2) WSL clang -O3 -mavx2 -mfma -I include <desug.c>  -> executable
#   3) WSL runs the clang executable  -> clang_out (stdout)
#   4) host tcc -run <src>             -> tcc_out  (golden)
#   5) compare clang_out vs tcc_out    -> byte-equal => PASS
# Usage:
#   powershell -File script/desugar.ps1 src.c [src.c ...] [-Keep] [-CC c] [-Sysroot s]
param(
    [Parameter(Position=0, ValueFromRemainingArguments=$true)][string[]]$Files,
    [switch]$Keep,
    [string]$CC = "clang",
    [string]$Sysroot = ""
)
$BASE = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
# emit-c hypervisor (cross, no -run) + native golden (-run available)
$EMITT = Join-Path $BASE "build\tcc-dg8.exe"
$TCC   = Join-Path $BASE "build\tcc-win.exe"
$INC   = @("-I", $BASE,
                "-I", (Join-Path $BASE "include"),
                "-I", (Join-Path $BASE "lib"),
                "-I", (Join-Path $BASE "src\posix\musl-nt64\include"),
                "-I", (Join-Path $BASE "src\posix\musl-nt64\arch\nt64"))
$OUT  = Join-Path $BASE "build\desugar"
New-Item -ItemType Directory -Force -Path $OUT | Out-Null
$PASS = 0; $FAIL = 0

if (-not $Files -or $Files.Count -eq 0) {
    Write-Output "usage: desugar.ps1 <src.c...> [-CC c] [-Sysroot s]"; exit 1
}
if (-not (Test-Path $EMITT)) { Write-Output "error: $EMITT not found"; exit 1 }

function To-WslPath([string]$p) {
    $p = $p -replace '\\','/'
    return "/mnt/" + $p.Substring(0,1).ToLower() + $p.Substring(2)
}

foreach ($s in $Files) {
    $src = Join-Path $BASE $s
    if (-not (Test-Path $src)) { Write-Output "SKIP $s (no source)"; continue }
    $b = [IO.Path]::GetFileNameWithoutExtension($s)
    $desc   = Join-Path $OUT "${b}.desug.c"
    $cerr   = Join-Path $OUT "${b}.emit.err"
    $gccerr = Join-Path $OUT "${b}.cc.err"
    $cldout = Join-Path $OUT "${b}.clang.out"
    $ccout  = Join-Path $OUT "${b}.tcc.out"
    $exe    = Join-Path $OUT "${b}.clang"

    # 1) host tcc emit-c (脱糖专用宏: simd.h 走 clang 侧 __m128, 产物=标准C+immintrin)
    & $EMITT --emit-c $src -o $desc @INC -D__TCC_DESUGAR__ 2>$cerr
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $desc)) {
        $FAIL++; Write-Output "EMIT-FAIL $b"; Get-Content $cerr -ErrorAction SilentlyContinue | Select-Object -First 10; continue
    }
    # 2)+3) WSL clang compile and run
    $wsrc   = To-WslPath $desc
    $winc   = To-WslPath (Join-Path $BASE "include")
    $wccerr = To-WslPath $gccerr
    $wcld   = To-WslPath $cldout
    $wexe   = To-WslPath $exe
    $wbase  = To-WslPath $BASE
    $sysarg = ""
    if ($Sysroot) { $sysarg = "--sysroot $Sysroot" }
    # 正式产物质量门禁 (2026-08-25): -Wall -Werror —— 独立库导出时合成机制 (static
    # 实例/反射表/operator 改名) 若丢 unused 保护或顺序错位, 编正式产物即失败.
    # 数值一致性门禁: -ffp-contract=off 令 clang 不做 mul+add 自动 FMA 收缩,
    # 与宿主 tcc (-run, 无自动 FMA) 的舍入一致 —— 否则小容差错序 (float K=130
    # 量级~13000, t089 #1) 会因 FMA 抖动超容差. 显式 _mm_fmadd 内在不受影响.
    #
    # 内联语义: clang -O2/-O3 内联 mt_mat_prod 后, 会基于 const 入参别名做优化,
    # 误把就地乘(dst==a==b, t089 #6)的「重叠备份→重算」错序, 破坏 A=A·A。
    # 该函数在源码已 __attribute__((noinline)) 阻断(clang 内联跨函数被禁),
    # 故此处不需额外 flag。若未来撤除 noinline, 需在此加 -fno-inline-functions。
    # WSL clang 命令: 三段式（编译 → 运行）打成 bash 脚本文件, 再 `wsl -e bash <sh>`.
    # 为何不直传 bash -lc 串: PS5.1 原生参数转义对嵌双引号的整串不可靠(`-c: option
    # requires an argument` 间歇复现, clang 假成功/不执行), PS7 同理脆弱. 脚本文件
    # 走 WSL 路径(LF/无BOM)执行, 完全绕开 PowerShell 参数转义, 跨 PS5.1/7 均稳定。
    # (2026-08-28 定版, 前序单引号+无外套引号方案仍间歇告警, 弃用)
    # -D_GNU_SOURCE: emit-c 丢弃源码里的 #define 宏, 而 musl 头(GNU扩展如 strcasestr)
    # 需此 feature 宏才暴露声明 —— clang(-I include 见同一套 include/)靠它对齐 tcc 环境。
    $scr = Join-Path $OUT "${b}.run.sh"
    $cmd = "cd $wbase; $CC -O3 -mavx2 -mfma -ffp-contract=off -Wall -Werror -D_GNU_SOURCE -lm -I '$winc' $sysarg '$wsrc' -o '$wexe' 2>'$wccerr' && '$wexe' > '$wcld' 2>&1"
    [IO.File]::WriteAllText($scr, $cmd + "`n", (New-Object System.Text.UTF8Encoding($false)))
    $wsh = To-WslPath $scr
    $env:MSYS_NO_PATHCONV = "1"
    wsl -e bash $wsh
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
        $FAIL++; Write-Output "CLANG-FAIL $b"; Get-Content $gccerr -ErrorAction SilentlyContinue | Select-Object -First 20
        if (-not $Keep) { Remove-Item $scr -Force -ErrorAction SilentlyContinue }
        continue
    }
    if (-not $Keep) { Remove-Item $scr -Force -ErrorAction SilentlyContinue }
    # 4) host native tcc -run golden (extensions run natively in tcc-win)
    #    编码: tcc 输出 UTF-8, PS 5.1 默认按 ANSI 捕获会乱码, `>` 又写 UTF-16 →
    #    捕获前设 [Console]::OutputEncoding=UTF8, 再显式写 UTF-8 无 BOM,
    #    与 WSL clang 的 UTF-8 输出字节一致 (t052 中文输出验证).
    Push-Location $BASE
    $prevEnc = [Console]::OutputEncoding
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
    $tccout = & $TCC -run @INC $src 2>"$OUT/$b.tcc.err"
    $tccrc = $LASTEXITCODE
    [Console]::OutputEncoding = $prevEnc
    Pop-Location
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    if ($null -ne $tccout) { [IO.File]::WriteAllText($ccout, ($tccout -join "`n") + "`n", $utf8) }
    else { [IO.File]::WriteAllText($ccout, "", $utf8) }
    # 5) compare (normalize CRLF vs LF; ignore stderr warnings)
    if ($tccrc -eq 0 -and (Test-Path $cldout) -and (Test-Path $ccout)) {
        $a = ([IO.File]::ReadAllText($cldout)).Replace("`r`n","`n").TrimEnd()
        $x = ([IO.File]::ReadAllText($ccout)).Replace("`r`n","`n").TrimEnd()
        if ($a -ceq $x) { $PASS++; Write-Output "PASS $b (clang output == tcc -run)" }
        else { $FAIL++; Write-Output "FAIL ${b}: output mismatch"; Write-Output "  tcc  : $x"; Write-Output "  clang: $a" }
    } else {
        $FAIL++; Write-Output "FAIL ${b}: tcc -run rc=$tccrc"
    }
    if (-not $Keep) { Remove-Item $desc,$gccerr,$cldout,$ccout,$exe,"$OUT/$b.tcc.err" -Force -ErrorAction SilentlyContinue }
}
Write-Output "=== desugar/clang: $PASS passed, $FAIL failed ==="
exit ($(if ($FAIL -eq 0) {0} else {1}))