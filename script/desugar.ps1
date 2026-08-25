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
    $desc   = Join-Path $OUT "$b.desug.c"
    $cerr   = Join-Path $OUT "$b.emit.err"
    $gccerr = Join-Path $OUT "$b.cc.err"
    $cldout = Join-Path $OUT "$b.clang.out"
    $ccout  = Join-Path $OUT "$b.tcc.out"
    $exe    = Join-Path $OUT "$b.clang"

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
    $cmd = "cd $wbase; $CC -O3 -mavx2 -mfma -I `"$winc`" $sysarg `"$wsrc`" -o `"$wexe`" 2>`"$wccerr`" && `"$wexe`" > `"$wcld`" 2>&1"
    $env:MSYS_NO_PATHCONV = "1"
    wsl -e bash -lc $cmd
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
        $FAIL++; Write-Output "CLANG-FAIL $b"; Get-Content $gccerr -ErrorAction SilentlyContinue | Select-Object -First 20; continue
    }
    # 4) host native tcc -run golden (extensions run natively in tcc-win)
    Push-Location $BASE
    & $TCC -run @INC $src > $ccout 2>"$OUT/$b.tcc.err"
    $tccrc = $LASTEXITCODE
    Pop-Location
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