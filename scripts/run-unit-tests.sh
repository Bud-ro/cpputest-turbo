#!/bin/sh
# Builds and runs everything under tests/. Placeholder until Phase 1.4 wires
# in real suites: succeeds with a notice when no tests exist yet.
set -eu
cd "$(dirname "$0")/.."

if [ -z "$(find tests -name '*.c' -o -name '*.cpp' 2>/dev/null | head -1)" ]; then
    echo "(no unit tests yet — Phase 1.4)"
    exit 0
fi

echo "ERROR: tests exist but run-unit-tests.sh has no runner wired up" >&2
exit 1
