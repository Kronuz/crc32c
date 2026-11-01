# AGENTS.md

Working notes for agents modifying this repository. Design is in `ARCHITECTURE.md`,
usage in `README.md` and `docs/API.md`.

## Repo map

```
include/crc32c/crc32c.hh   The library: Crc32c, crc32c(), crc32c_combine(), detail:: routines. Header only.
test/crc32c_test.cc        Vectors, hardware vs table and vs the bitwise oracle at every length/alignment/state around every tier boundary, fragments, empty updates, combine, detection before main.
test/consumer/             A project that links only crc32c::crc32c with fetching disabled (CTest crc32c_consumer).
fuzz/fuzz_update.cc        libFuzzer target; fuzz/replay.cc runs it as a test (CTest crc32c_fuzz_replay).
examples/demo.cc           A tour: one shot, incremental, constexpr, combine, resume, which path runs.
bench/crc32c.cc            GB/s: hardware vs a plain instruction loop vs slice-by-8 vs byte table (-DCRC32C_BUILD_BENCHMARKS=ON).
CMakeLists.txt             INTERFACE target crc32c::crc32c; tests/examples/bench only when top-level.
.github/workflows/ci.yml   The shared cmake workflow, plus a per-runner job that requires the hardware path.
```

## Build and test

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON && cmake --build build
ctest --test-dir build --output-on-failure
CRC32C_REQUIRE_HARDWARE=1 ./build/crc32c_test     # fails if the CPU's instructions are not used
```

Sanitizers: add `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"`. Fuzz: Clang with `-DCRC32C_FUZZ=ON`.

## Invariants (do not regress these)

- **Standard library only.** No other include, no FetchContent: a `crc32c`-only consumer
  fetches nothing (`crc32c_consumer` pins it). hashes and journal both depend on this.
- **The value never changes.** Polynomial `0x82f63b78`, init and final XOR `~0`. Journal
  and strata persist these CRCs; journal's byte fixtures (`test/fixtures/crc32c`) must
  keep verifying. Any change to a path must keep hardware, table and bitwise equal.
- **The hardware loop never reads past `n`.** Tails go byte by byte; test with exact-size
  heap blocks under ASan when touching it.
- **Empty updates are the identity** at any state, including a null pointer with size 0.
- **No global target flags.** Hardware code lives in target-attributed functions only.
- **`Crc32c` keeps the inverted register.** Resume (`Crc32c(v)`) and combine rely on it.
- **The lane sizes are measured, per architecture.** `lane_large`, `lane_middle` and
  `lane_small` in the header (and so `smallest_stride`, where the plain loop gives way) come
  from an A/B at every size on an Intel Core i9 and Apple M-series cores. Change one only
  with the same A/B on both architectures; a stride that is too short loses to the plain
  loop (a merge per few steps), one that is too long leaves a long remainder on the slow chain.
- **No tier may fall behind the loop below it.** The plain loop is the baseline: `crc32c_bench`
  prints it as `loop`, and `crc32c()` must match it below the smallest stride.

## Gotchas

- `CRC32C_X86` / `CRC32C_ARM` leak from the header (they gate the test's choice of
  routine); keep them prefixed.
- Detection calls `__builtin_cpu_init()` before `__builtin_cpu_supports`: on macOS the CPU
  model is filled in by a constructor that runs after the namespace-scope objects of the
  program's own files, so a CRC taken while those are constructed would cache "no
  instructions" for the whole run. The test pins it (`detected_before_main`).
- The 4 KiB shift tables (`shift_by<K>`) are built at compile time, two per lane tier.
- A CPU without the instructions runs the table path and the hardware test says
  `hardware no` and passes: that is why CI sets `CRC32C_REQUIRE_HARDWARE=1` per runner.
- Names `kronuz::crc32c::crc32c` and the namespace share a spelling; inside
  `kronuz::journal`-like scopes that declare their own `crc32c`, call it fully qualified
  (`::kronuz::crc32c::crc32c(...)`).
- The header is `crc32c/crc32c.hh`, not a bare `crc32c.hh`: older hashes commits ship a
  `crc32c.hh` of their own (namespace `hashes`), and two headers of one name on one
  include path would shadow each other. The two can coexist in one closure.

## Consumers

journal (`journal::crc32c`, `journal::Checksum`), strata and residency. hashes no longer
carries CRC-32C. Pin by commit.
