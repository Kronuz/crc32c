# crc32c

[![CI](https://github.com/Kronuz/crc32c/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/crc32c/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

CRC-32C (Castagnoli) for C++20: hardware instructions on x86-64 (SSE4.2) and ARMv8,
chosen at run time, a slice-by-8 table everywhere else and at compile time, and
`crc32c_combine`. One header, no dependencies. It is the checksum of iSCSI, ext4,
Btrfs and `kronuz::journal`.

## Highlights

- **Hardware where the CPU has it.** `_mm_crc32_u64` on x86-64 and `__crc32cd` on
  AArch64, in functions compiled with the target attribute, so no global `-msse4.2`
  or `-march` flag is needed. Detection runs once.
- **Three interleaved lanes** for buffers of 3 KiB and more, hiding the instruction's
  latency; the lanes merge with shift constants computed at compile time.
- **`constexpr`.** `static_assert(crc32c("123456789") == 0xe3069283)` works.
- **Incremental and joinable.** `Crc32c` takes any number of pieces (empty ones
  included) and can resume from a stored CRC; `crc32c_combine` gives the CRC of A‖B
  from the two CRCs and B's length without rereading.
- **No dependencies.** Standard library only; nothing is fetched.

## Installation

Requires a C++20 compiler and CMake 3.14 or newer.

```cmake
include(FetchContent)
FetchContent_Declare(crc32c
  GIT_REPOSITORY https://github.com/Kronuz/crc32c.git
  GIT_TAG        <commit>)
FetchContent_MakeAvailable(crc32c)

target_link_libraries(my_app PRIVATE crc32c::crc32c)
```

## Quick start

```cpp
#include <crc32c/crc32c.hh>

namespace c32 = kronuz::crc32c;

std::uint32_t c = c32::crc32c(data, size);                  // one shot
c32::Crc32c crc;                                            // incremental
crc.update(header, header_size).update(body, body_size);
static_assert(c32::crc32c("123456789") == 0xe3069283);      // constexpr
auto both = c32::crc32c_combine(crc_a, crc_b, size_b);      // CRC of A‖B without rereading
auto more = c32::Crc32c(stored).update(next, n).value();    // resume from a stored CRC
```

`examples/demo.cc` runs all of these and prints which path is in use. The full
interface is in [docs/API.md](docs/API.md).

## Building and testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/crc32c_bench          # GB/s: hardware, slice-by-8, byte table
```

The tests check the standard vectors, the hardware routine against the table routine
at every length, alignment and starting state, every split around the lane and stride
boundaries, random fragmentation with empty pieces, combine, and a consumer project
that links only `crc32c::crc32c` with fetching disabled. The test prints
`crc32c: hardware yes|no`; `CRC32C_REQUIRE_HARDWARE=1` makes `no` a failure, which CI
sets on runners whose CPU has the instructions. `-DCRC32C_FUZZ=ON` with Clang builds
a libFuzzer target; without it a replay driver runs the same checks as a test.

## Where it came from

CRC-32C lived in [hashes](https://github.com/Kronuz/hashes) (`crc32c.hh`, namespace
`hashes`) and as a byte table in [journal](https://github.com/Kronuz/journal). This is
the one implementation; its history is the file's history in hashes. To migrate from
hashes, include `<crc32c/crc32c.hh>` and replace `hashes::` with `kronuz::crc32c::`
(`crc32c`, `Crc32c`, `crc32c_combine`); the values are the same.

## License

MIT, Copyright (c) 2026 Germán Méndez Bravo.
