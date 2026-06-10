# Contributing to cpputest-turbo

## Ground rules

- `third_party/cpputest/` is the vendored upstream **oracle**. It is
  read-only: never edit it, always consult it. Upstream's *sources* are the
  spec for behavior and output — byte-for-byte, quirks included.
- Output parity is non-negotiable. If our output differs from upstream's
  for the same public-API input, it's our bug (or a documented divergence
  in the README).
- The core is C11. Public headers under `include/CppUTest*/` may be C++ but
  must stay thin — no heavy templates, no deep include chains.

## The gate

```sh
./scripts/check.sh
```

runs everything a PR must pass: format/hygiene check, build (max-strict
warnings under `-Werror`), our unit suites, the upstream conformance suite,
the ASan/UBSan sweep, and a bounded differential fuzz pass. `CHECK_FAST=1`
skips the heavy stages during inner-loop development; run the full gate
before pushing. `scripts/check-macos.sh` cross-compiles for both macOS
architectures if you have `zig` available.

## Style

`clang-format` (config in `.clang-format`) over `src/`, `include/`,
`fuzz/`, `conformance/`:

```sh
./scripts/check-format.sh --fix
```

Test suites under `tests/` are exempt from reflowing: golden files embed
their `file:line` locations, so reformatting them invalidates goldens.
Hygiene rules (no tabs, no trailing whitespace, final newlines) apply
everywhere.

## Golden files

Golden outputs under `tests/*/golden/` are byte-exact captures verified
against upstream. If a change legitimately alters output, regenerate the
golden AND prove the new output matches upstream (compile the same test
source against the vendored upstream library and diff — see
`scripts/check-fuzz.sh` for the pattern). Never hand-edit a golden to make
a test pass.

## Differential fuzzing

`fuzz/fuzz_mock_diff.cpp` compiles against both upstream and us; identical
seeds must produce identical output. To reproduce a CI divergence:

```sh
FUZZ_SEED=<n> FUZZ_TRACE=1 ./build/fuzz/mockdiff_ours -sn seq<k> 2>trace.txt
```

`FUZZ_ROUNDS` deepens the soak, `FUZZ_OPT` changes the optimization level
(CI runs -O0 through -O3).

## Commit etiquette

Every commit must leave `./scripts/check.sh` green. Behavioral parity
fixes should cite the upstream source location they mirror
(file:line in `third_party/cpputest/`).
