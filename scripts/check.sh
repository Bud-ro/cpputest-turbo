#!/bin/sh
# One command that must be green at the end of every loop iteration:
# builds the libraries and runs every suite/gate (lite: no conformance —
# the vendored-upstream differential fuzzers are the behavioral oracle).
set -eu
cd "$(dirname "$0")/.."

echo "== format/lint =="
./scripts/check-format.sh

echo "== build =="
make all

echo "== unit tests =="
./scripts/run-unit-tests.sh

echo "== vendor smoke (nested third_party build) =="
./scripts/check-vendor.sh

# the heavier gates run by default; CHECK_FAST=1 skips them for quick loops
if [ "${CHECK_FAST:-0}" != "1" ]; then
    echo "== static analyzer =="
    ./scripts/check-analyzer.sh

    echo "== sanitizers =="
    ./scripts/check-sanitizers.sh

    echo "== fuzz (bounded; FUZZ_ROUNDS env to deepen) =="
    FUZZ_ROUNDS="${FUZZ_ROUNDS:-3}" FUZZ_ITERS="${FUZZ_ITERS:-5000}" ./scripts/check-fuzz.sh
fi

echo "== check.sh: ALL GREEN =="
