# Benchmark results (lite branch)

Machine: Linux x86_64 (WSL2), gcc 13.3. Reproduce with `sh bench/run_bench.sh`
(builds the vendored upstream library for comparison on first run). Updated
2026-06-11 on the `lite` branch — no leak detection, so `new`/`delete` in
tests run at plain libc speed and `TestHarness.h` pre-includes no C++
standard headers.

## Compile time (the lite branch's headline win)

| TU shape | preprocessed lines | compile (g++ -O2, warm) |
|---|---|---|
| `TestHarness.h` only — upstream/master | 30,789 | ~120 ms |
| `TestHarness.h` only — **lite** | **1,006** | **~20 ms** |
| + `MockSupport.h` — upstream/master | 32,357 | ~590 ms |
| + `MockSupport.h` — **lite** | **2,454** | **~450 ms** |

Master and upstream pay for `<new>`/`<memory>`/`<string>`, which
`TestHarness.h` must pre-include before defining the leak-detection `new`
macro. Lite has no leak detection, so that entire tax is gone; what remains
of the mock-TU cost is `MockSupport.h`'s own inline facade bodies.

Suite-level (`bench/run_bench.sh`, 21 TUs / 400 TESTs, -O0):

| | cpputest-turbo lite | upstream |
|---|---|---|
| total | **1,152 ms** | 2,804 ms |

## Runtime (5M assertions + 500k new/delete, 8 groups, -O2)

| run | time | vs upstream |
|---|---|---|
| upstream CppUTest, sequential | ~31 ms | 1× |
| **lite, sequential** | **~4 ms** | **~8×** |

(master with leak tracking measured 6–10 ms on the same load; lite's
untracked `new`/`delete` removes that overhead entirely. `-jN` still
distributes groups over forked workers for larger suites.)

## Mock path (bench/bench_mock.cpp, -O2)

200k expectOneCall/actualCall pairs (int + string params, returnIntValue)
plus 50k custom-type (comparator) pairs, with periodic
checkExpectations+clear — the hot loop of a mock-heavy embedded suite:

| run | time | vs upstream |
|---|---|---|
| upstream CppUMock | 334–339 ms | 1× |
| **cpputest-turbo** | **135 ms** | **~2.5×** |

The mock numbers carry over from master: the mock core is the same code
minus the data store and tracing mode, neither of which is on the hot path.

## What made it fast

1. **Inline assertion fast path.** The passing path of every hot macro
   (CHECK/LONGS_EQUAL/STRCMP_EQUAL/MEMCMP_EQUAL/DOUBLES_EQUAL/...) is a
   counter increment plus an inlined comparison in the user's translation
   unit; the library is only entered on mismatch, where the byte-identical
   failure messages are built. String/memory comparisons happen inside the
   same full expression that evaluated the arguments, so temporaries stay
   alive — same lifetime rules as upstream's call-form macros.
2. **No allocation tracking.** Lite removes the leak detector outright, so
   `new`/`delete` and `malloc`/`free` in tests are the plain libc calls.
3. **`-jN`** distributes groups over forked workers; on this load 8 workers
   give another ~2.5–3× (the wall time includes fork/replay overhead, so
   larger suites scale better).
