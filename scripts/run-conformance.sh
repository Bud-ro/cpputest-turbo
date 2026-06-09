#!/bin/sh
# Builds upstream's public-API tests against OUR headers + libs and runs them.
# Placeholder until Phase 7 wires in the real harness.
set -eu
cd "$(dirname "$0")/.."

if [ ! -f conformance/manifest.txt ]; then
    echo "(no conformance manifest yet — Phase 7)"
    exit 0
fi

echo "ERROR: conformance manifest exists but run-conformance.sh has no harness" >&2
exit 1
