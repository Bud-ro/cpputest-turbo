# Benchmark results

Machine: Linux x86_64 (WSL2), gcc 13.3. Reproduce with `sh bench/run_bench.sh`
(builds the vendored upstream library for comparison on first run). Updated
2026-06-09 after the fast-path/allocator optimization rounds.

## Runtime (5M assertions + 500k tracked new/delete, 8 groups, -O2)

| run | time | vs upstream |
|---|---|---|
| upstream CppUTest, sequential | 29 ms | 1× |
| **cpputest-revibed, sequential** | **5–6 ms** | **~5×** |
| **cpputest-revibed, `-j8`** | **2 ms** | **~14×** |
| cpputest-revibed, `-p` (fork per test) | 6 ms | ~5× |

Decomposed microbenchmarks (per-operation cost):

| operation | before | after | upstream | raw-C floor |
|---|---|---|---|---|
| passing assertion | 2.7 ns | **~0.3 ns** | 3.8 ns | ~0.2 ns |
| tracked new/delete pair | 14 ns | **6.4 ns** | 17.6 ns | ~2 ns |

## What made it fast

1. **Inline assertion fast path.** The passing path of every hot macro
   (CHECK/LONGS_EQUAL/STRCMP_EQUAL/MEMCMP_EQUAL/DOUBLES_EQUAL/...) is now a
   counter increment plus an inlined comparison in the user's translation
   unit; the library is only entered on mismatch, where the byte-identical
   failure messages are built. (10× on the assertion microbenchmark.)
   String/memory comparisons happen inside the same full expression that
   evaluated the arguments, so temporaries stay alive — same lifetime rules
   as upstream's call-form macros.
2. **Single-allocation leak tracking.** The tracking node is embedded at the
   head of the tracked block (`[node | user | guard]`) — one malloc/free per
   allocation instead of two.
3. **Size-class freelists.** Small tracked blocks recycle through capped
   per-class freelists, so steady-state new/delete churn skips malloc
   entirely. (14 ns → 6.4 ns per pair; remaining cost is the 0xCD poison,
   guard verification and hash bookkeeping that parity requires.)
4. **`-jN`** distributes groups over forked workers; on this load 8 workers
   give another ~2.5–3× (the 2 ms wall includes ~1 ms of fork/replay
   overhead, so larger suites scale better).

## Compile time (21 TUs / 400 TESTs, g++ -O0)

| | cpputest-revibed | upstream |
|---|---|---|
| total | 2411 ms | 2421 ms |

Parity: both frameworks' `TestHarness.h` must pre-include `<new>`,
`<memory>` and `<string>` before defining the `new` macro, and those std
headers dominate (~31k preprocessed lines on both sides). Our headers keep
method bodies out-of-line (`src/shim/`) to stay at parity. Pure-C test files
(`TestHarness_c.h` only) skip the C++ standard library entirely and compile
in a few tens of milliseconds — upstream has no equivalent.
