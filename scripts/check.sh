#!/bin/sh
# One command that must be green at the end of every loop iteration:
# builds the libraries, runs our unit tests, runs the conformance suite.
set -eu
cd "$(dirname "$0")/.."

echo "== build =="
make all

echo "== unit tests =="
./scripts/run-unit-tests.sh

echo "== conformance =="
./scripts/run-conformance.sh

# the heavier gates run by default; CHECK_FAST=1 skips them for quick loops
if [ "${CHECK_FAST:-0}" != "1" ]; then
    echo "== sanitizers =="
    ./scripts/check-sanitizers.sh

    echo "== fuzz (bounded; FUZZ_ROUNDS env to deepen) =="
    FUZZ_ROUNDS="${FUZZ_ROUNDS:-3}" FUZZ_ITERS="${FUZZ_ITERS:-5000}" ./scripts/check-fuzz.sh
fi

echo "== check.sh: ALL GREEN =="
