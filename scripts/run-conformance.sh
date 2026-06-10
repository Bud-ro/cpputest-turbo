#!/bin/sh
# Conformance: bundle every file in conformance/manifest.txt (upstream test
# sources, UNMODIFIED) with our headers + libs into one binary and run it.
set -eu
cd "$(dirname "$0")/.."

if [ ! -f conformance/manifest.txt ]; then
    echo "FAILED: conformance/manifest.txt is missing (bad checkout?)" >&2
    exit 1
fi

CXX="${CXX:-g++}"
BUILD=build/conformance
mkdir -p "$BUILD"

SOURCES="conformance/conformance_main.cpp"
while IFS= read -r line; do
    case "$line" in
        \#*|"") continue ;;
    esac
    SOURCES="$SOURCES third_party/cpputest/$line"
done < conformance/manifest.txt

# -w: upstream sources carry their own warning baggage; we compile them
# unmodified and only require behavior, not warning-cleanliness.
$CXX -std=c++11 -w -O1 -g -Iinclude $SOURCES \
    build/libCppUTestExt.a build/libCppUTest.a -o "$BUILD/conformance_tests"

rc=0
out=$("$BUILD/conformance_tests" 2>&1) || rc=$?
printf '%s\n' "$out"
if [ "$rc" -ne 0 ]; then
    echo "FAILED: conformance suite exit $rc" >&2
    exit 1
fi
echo "conformance: upstream tests green"
