# Benchmark results

Machine: Linux x86_64 (WSL2), gcc 13.3. Reproduce with `sh bench/run_bench.sh`
(builds the vendored upstream library for comparison on first run).

Three consecutive runs, 2026-06-09:

| metric | cpputest-revibed | upstream CppUTest |
|---|---|---|
| compile 21 TUs / 400 TESTs (g++ -O0) | 2413–2472 ms | 2400–2511 ms |
| run 2.5M assertions + 250k new/delete (-O2) | **11–12 ms** | 15 ms |

## Reading the numbers

- **Runtime: ~25–30% faster.** The C core's assertion hot path does no heap
  allocation and no virtual dispatch on the passing path; the leak tracker
  uses a 1024-bucket hash table. Most of the remaining time in both
  frameworks is the leak detector bookkeeping for new/delete.
- **C++ compile time: parity.** Both frameworks' `TestHarness.h` must
  pre-include `<new>`, `<memory>` and `<string>` before defining the `new`
  macro (that protection is part of the compatible interface), and those
  std headers dominate the ~31k preprocessed lines on both sides. Our shim
  keeps method bodies out of the headers (`src/shim/simplestring.cpp`), which
  brought per-TU cost from ~15% worse to parity.
- **Pure-C consumers are where compilation wins big**: a C test TU includes
  only `TestHarness_c.h` + the C core header — no C++ standard library at
  all — which upstream cannot offer since its core is C++.
