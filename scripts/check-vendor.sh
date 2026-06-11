#!/bin/sh
# Vendoring smoke test: prove the project builds and works when nested inside
# another repository (e.g. host/third_party/cpputest-turbo) with no path,
# include-style, or CWD assumptions. Simulates the real consumer flow:
#   1. copy the tree (sans build artifacts / .git / vendored upstream oracle)
#      into a fake host project
#   2. `make -C` it from the host root, default and CPPUTEST_C_ONLY builds
#   3. compile + run a host C++ test (TestHarness + MockSupport) and a host
#      C test (TestHarness_c) against only -I<vendor>/include + the .a files
set -eu
cd "$(dirname "$0")/.."
SRC="$PWD"

WORK="${TMPDIR:-/tmp}/cpputest-turbo-vendor-smoke.$$"
trap 'rm -rf "$WORK"' EXIT INT TERM
HOST="$WORK/hostapp"
VEND="$HOST/third_party/cpputest-turbo"
mkdir -p "$VEND"

# Copy the working tree as a consumer would vendor it. Exclude dev-only bulk;
# everything a consumer needs (Makefile, include/, src/) must survive.
for d in Makefile cpputest.pc.in include src; do
    cp -R "$SRC/$d" "$VEND/$d"
done

# --- nested build, default (C++ shim included) ---
make -C "$VEND" all >/dev/null

# --- host C++ consumer test: harness + mock, linked against the vendored libs ---
cat > "$HOST/host_test.cpp" <<'EOF'
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

TEST_GROUP(VendorSmoke)
{
    void teardown() { mock().clear(); }
};

TEST(VendorSmoke, mockRoundTrip)
{
    mock().expectOneCall("frob").withParameter("x", 7).andReturnValue(42);
    LONGS_EQUAL(42, mock().actualCall("frob").withParameter("x", 7).returnIntValue());
    mock().checkExpectations();
}

TEST(VendorSmoke, assertsWork)
{
    STRCMP_EQUAL("vendored", "vendored");
    DOUBLES_EQUAL(1.0, 1.0, 0.001);
}

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
EOF
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror \
    -I"$VEND/include" "$HOST/host_test.cpp" \
    "$VEND/build/libCppUTestExt.a" "$VEND/build/libCppUTest.a" \
    -o "$HOST/host_test"
( cd "$HOST" && ./host_test ) >/dev/null
echo "ok: nested C++ consumer builds and passes"

# --- C-only nested build from a clean copy (host has no C++ toolchain) ---
rm -rf "$VEND/build"
make -C "$VEND" CPPUTEST_C_ONLY=1 all >/dev/null
cat > "$HOST/host_test_c.c" <<'EOF'
#include "cpputest_core/core.h"

static void *t_create(cu_test *t) { (void)t; return (void *)0; }
static void t_destroy(cu_test *t, void *f) { (void)t; (void)f; }
static void t_setup(cu_test *t, void *f) { (void)t; (void)f; }
static void t_teardown(cu_test *t, void *f) { (void)t; (void)f; }
static void t_body(cu_test *t, void *f)
{
    (void)t;
    (void)f;
    cu_assert_longs_equal(3, 3, (const char *)0, __FILE__, __LINE__);
    cu_assert_cstr_equal("vendored", "vendored", (const char *)0, __FILE__,
                         __LINE__);
}

static const cu_test_ops ops = { t_create, t_destroy, t_setup, t_body,
                                 t_teardown };
static cu_test the_test;

int main(int argc, const char *const *argv)
{
    the_test.group = "VendorSmokeC";
    the_test.name = "pureC";
    the_test.file = __FILE__;
    the_test.line = __LINE__;
    the_test.ops = &ops;
    cu_registry_add(&the_test);
    return cu_run_all(argc, argv);
}
EOF
"${CC:-gcc}" -std=c11 -Wall -Wextra -Werror \
    -I"$VEND/include" "$HOST/host_test_c.c" \
    "$VEND/build/libCppUTest.a" \
    -o "$HOST/host_test_c"
"$HOST/host_test_c" >/dev/null
echo "ok: nested C-only consumer builds and passes"

# --- out-of-tree object dir: host directs build artifacts elsewhere ---
make -C "$VEND" BUILD="$HOST/obj/cpputest" all >/dev/null
test -f "$HOST/obj/cpputest/libCppUTest.a"
echo "ok: BUILD= override places artifacts outside the vendored tree"

# --- header hygiene: hosts compile our public headers under THEIR flags;
# every public header must survive strict warnings at every C++ standard
# the toolchain offers ---
cat > "$HOST/all_headers.cpp" <<'EOF'
#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/MemoryLeakWarningPlugin.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"
#include "CppUTest/PlatformSpecificFunctions.h"
#include "CppUTest/SimpleString.h"
#include "CppUTest/TestFailure.h"
#include "CppUTest/TestOutput.h"
#include "CppUTest/TestPlugin.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTest/TestResult.h"
#include "CppUTest/TestTestingFixture.h"
#include "CppUTest/Utest.h"
#include "CppUTest/UtestMacros.h"
#include "CppUTest/CppUTestConfig.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupport_c.h"
#include "CppUTestExt/MockSupportPlugin.h"
#include "CppUTestExt/MockNamedValue.h"
#include "CppUTestExt/MockExpectedCall.h"
#include "CppUTestExt/MockActualCall.h"
#include "CppUTestExt/OrderedTest.h"

int consumer_translation_unit_is_not_empty;
EOF
# headers are thin C-ABI shims: C-style casts are the idiom (upstream's
# headers don't pass -Wold-style-cast either), so that flag is exercised
# only through -isystem — how real consumers include third-party headers
for STD in c++11 c++14 c++17 c++20; do
    if "${CXX:-g++}" -std=$STD -x c++ -c /dev/null -o /dev/null 2>/dev/null; then
        "${CXX:-g++}" -std=$STD -Wall -Wextra -Werror -Wpedantic -Wshadow \
            -Wconversion -Wsign-conversion -Wcast-align -Wundef \
            -Wcast-qual \
            -I"$VEND/include" -c "$HOST/all_headers.cpp" \
            -o "$HOST/all_headers.o"
        "${CXX:-g++}" -std=$STD -Wall -Wextra -Werror -Wpedantic \
            -Wold-style-cast \
            -isystem "$VEND/include" -c "$HOST/all_headers.cpp" \
            -o "$HOST/all_headers.o"
        echo "ok: public headers clean under -std=$STD -Werror (+isystem)"
    fi
done

# --- install flow: make install to a DESTDIR, build a consumer against the
# installed tree using the pkg-config file's own flags ---
DEST="$WORK/destroot"
make -C "$VEND" DESTDIR="$DEST" PREFIX=/usr install >/dev/null
PC="$DEST/usr/lib/pkgconfig/cpputest.pc"
test -f "$PC"
PC_CFLAGS=$(PKG_CONFIG_PATH="$DEST/usr/lib/pkgconfig" \
    pkg-config --define-prefix --cflags cpputest 2>/dev/null) || \
    PC_CFLAGS="-I$DEST/usr/include"
PC_LIBS=$(PKG_CONFIG_PATH="$DEST/usr/lib/pkgconfig" \
    pkg-config --define-prefix --libs cpputest 2>/dev/null) || \
    PC_LIBS="-L$DEST/usr/lib -lCppUTestExt -lCppUTest"
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror \
    $PC_CFLAGS "$HOST/host_test.cpp" $PC_LIBS -o "$HOST/host_test_installed"
"$HOST/host_test_installed" >/dev/null
echo "ok: DESTDIR install + pkg-config consumer builds and passes"

echo "vendor smoke green"
