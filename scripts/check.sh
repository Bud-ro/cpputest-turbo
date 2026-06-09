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

echo "== check.sh: ALL GREEN =="
