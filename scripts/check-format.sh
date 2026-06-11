#!/bin/sh
# Formatting + hygiene gate.
#
# clang-format scope: library sources and headers, the fuzz harnesses, and
# the conformance driver. Test suites under tests/ are deliberately NOT
# formatted: golden files embed their file:line locations, so reflowing
# them would invalidate every golden.
#
# CLANG_FORMAT overrides the binary (CI installs clang-format; locally a
# static build from github.com/muttleyxd/clang-tools-static-binaries works).
# Pass --fix to rewrite files in place instead of checking.
set -eu
cd "$(dirname "$0")/.."

CF="${CLANG_FORMAT:-clang-format}"
MODE="--dry-run"
[ "${1:-}" = "--fix" ] && MODE="-i"

FILES=$(find src include fuzz -name '*.c' -o -name '*.h' -o -name '*.cpp' | sort)

fail=0

if command -v "$CF" >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    "$CF" $MODE --Werror $FILES || fail=1
else
    echo "check-format: clang-format not found; skipping style check" >&2
fi

# objective hygiene rules, checked everywhere (incl. tests/) with no tools
for f in $FILES $(find tests -name '*.c' -o -name '*.cpp' | sort); do
    if grep -q '[[:blank:]]$' "$f"; then
        echo "trailing whitespace: $f" >&2
        fail=1
    fi
    if grep -q "$(printf '\t')" "$f"; then
        echo "tab character in source: $f" >&2
        fail=1
    fi
    if [ -n "$(tail -c 1 "$f")" ]; then
        echo "missing final newline: $f" >&2
        fail=1
    fi
done

[ "$fail" = 0 ] && echo "format check green"
exit "$fail"
