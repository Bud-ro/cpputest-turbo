# cpputest-turbo

A from-scratch rewrite of CppUTest: C11 core + thin C++ header shim, source-compatible
with upstream. **PLAN.md is the single source of truth for what to do next** — read it
first, work the next unchecked item, verify, check it off, append to the iteration log,
and commit.

## Hard rules
- `third_party/cpputest/` is the vendored upstream oracle. **Read-only.** Never edit it;
  consult it for exact macro signatures, output strings, and behavior.
- Core is C11 (`-std=c11 -Wall -Wextra -Werror`, gcc). Public headers under `include/`
  may be C++ but must stay thin: no heavy templates, no deep include chains, no RAII or
  virtual dispatch unless upstream API compatibility forces it.
- Output/format parity matters: failure messages, summary lines, exit codes must match
  upstream byte-for-byte (upstream sources are the spec, not its docs).
- Every iteration must end with `scripts/check.sh` green and a commit. Never commit red.
- No cmake. POSIX make + gcc only for consumers. Zig 0.16 (`~/bin/zig`) is available for
  cross-compile smoke checks (`zig cc -target aarch64-macos` etc.), not as a requirement.

## Build & test
- `make` — builds `libCppUTest.a` (+ `libCppUTestExt.a` once mocks exist)
- `./scripts/check.sh` — builds everything, runs our tests + conformance suite
- `make conformance` — upstream public-API tests against our lib

## Environment notes
- Linux WSL2, gcc 13.3, GNU make, no cmake/clang installed. zig 0.16.0 at `~/bin/zig`.
- Platform targets: Linux + macOS. Guard platform-specific code; no glibc-only APIs
  without a fallback.
