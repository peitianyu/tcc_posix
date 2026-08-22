#!/bin/bash
# build_bt.sh - build TCC -bt stack-backtrace runtime objects (bt-exe.o, bt-log.o)
#               for the musl stress, and (bcheck.o for -b) into build/win-musl-obj
#               and build/lib.
#
# musl-only toolchain: no winapi tree.  lib/bt-inc supplies the minimal
# windows.h/io.h/direct.h shims so tccrun.c (which pulls in tcc.h -> <windows.h>)
# compiles against musl headers.  CONFIG_TCC_MUSL_STDIO routes the runtime's
# stdio calls (stderr/snprintf/vfprintf/fflush) to musl; CONFIG_TCC_SEMLOCK=0
# drops the winapi CRITICAL_SECTION path (single-threaded runtime).
#
# MUST run the SELF-HOSTED tcc (build/tcc-win.exe) with explicit -I so it finds
# the standard headers; it is NOT installed with a default include tree.
set -e
BASE="$(cd "$(dirname "$0")/.." && pwd)"
TCC="$BASE/build/tcc-win.exe"
SRC="$BASE/src"
MUSL="$SRC/posix/musl-nt64"
INC="$SRC/posix/musl-nt64/include"
BTINC="$BASE/lib/bt-inc"
OUT="$BASE/build/win-musl-obj"

DEFS="-DCONFIG_TCC_MUSL_STDIO=1 -DCONFIG_TCC_SEMLOCK=0 -DONE_SOURCE=1 -DCONFIG_TCC_MUSL=1"
INCARGS="-I$SRC -I$BTINC -I$INC"

# bcheck.o : boundary checker (-b).  compiled WITHOUT -b so its own malloc()
#            calls go straight to musl's real malloc (no recursion through
#            __bound_malloc).  CONFIG_TCC_MUSL selects the winapi/pthread/dlfcn
#            -free single-threaded configuration in bcheck.c.
"$TCC" -c $DEFS $INCARGS "$BASE/lib/bcheck.c" -o "$OUT/bcheck.o"

# bt-exe.o : crash backtrace runtime (links tccrun.c + exception handler)
"$TCC" -c $DEFS $INCARGS "$BASE/lib/bt-exe.c" -o "$OUT/bt-exe.o"

# bt-log.o : on-demand backtrace entry (tcc_backtrace)
"$TCC" -c $DEFS $INCARGS "$BASE/lib/bt-log.c" -o "$OUT/bt-log.o"

# refresh lib copies for the build-tree tcc
cp "$OUT/bcheck.o" "$BASE/build/lib/bcheck.o"
cp "$OUT/bt-exe.o" "$BASE/build/lib/bt-exe.o"
cp "$OUT/bt-log.o" "$BASE/build/lib/bt-log.o"
# refresh installed tcc (tccdir=bin/lib) copies so `tcc -b` finds them at link
cp "$OUT/bcheck.o" "$BASE/bin/lib/bcheck.o"
cp "$OUT/bt-exe.o" "$BASE/bin/lib/bt-exe.o"
cp "$OUT/bt-log.o" "$BASE/bin/lib/bt-log.o"
echo "OK: bcheck.o bt-exe.o bt-log.o -> $OUT build/lib bin/lib"