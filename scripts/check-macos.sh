#!/bin/sh
# Cross-compile smoke check for macOS (aarch64 + x86_64) using zig cc.
# Compile-only: validates no Linux-isms in the core or shim headers.
set -eu
cd "$(dirname "$0")/.."

ZIG="${ZIG:-zig}"
OUT="${TMPDIR:-/tmp}/cpputest-macos-check"
mkdir -p "$OUT"

for target in aarch64-macos x86_64-macos; do
    for f in src/core/*.c src/mock/*.c; do
        $ZIG cc -target "$target" -std=c11 -Wall -Wextra -Iinclude \
            -c "$f" -o "$OUT/c.o"
    done
    for f in src/shim/newdelete.cpp tests/smoke/smoke_tests.cpp \
             tests/mock/mock_tests.cpp tests/c_interface/c_wrappers.cpp; do
        $ZIG c++ -target "$target" -std=c++11 -Iinclude \
            -c "$f" -o "$OUT/cpp.o"
    done
    echo "ok: $target compiles"
done
echo "macOS cross-compile check green"
