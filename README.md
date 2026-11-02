# crc32c

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/crc32c/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/crc32c/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

CRC-32C for C++20: SSE4.2 and ARMv8 instructions picked at run time, slice-by-8 elsewhere,
constexpr and combine. The hardware routines run interleaved lanes and are compiled with
a function-level target attribute, so no global `-msse4.2` or `-march` flag is needed; a
slice-by-8 table covers other CPUs and compile time. CRC-32C (Castagnoli) is the checksum of
iSCSI, ext4, Btrfs and [journal](https://github.com/Kronuz/journal); reach for this one header
when you need it fast and dependency-free.

## ✨ Highlights

- **Hardware where the CPU has it.** `_mm_crc32_u64` on x86-64 and `__crc32cd` on AArch64,
  detected once at run time.
- **Interleaved lanes.** From 384 bytes on x86-64 (96 on AArch64) the buffer runs as strides
  of three lanes at once to hide the instruction's latency, in tiers of 1 KiB, 256-byte and
  128-byte lanes (32 on AArch64); shift tables built at compile time merge the lanes. Shorter
  buffers run one plain chain.
- **`constexpr`.** `static_assert(crc32c("123456789") == 0xe3069283)` compiles.
- **Incremental and joinable.** `Crc32c` takes any number of pieces, empty ones included, and
  resumes from a stored CRC; `crc32c_combine` gives the CRC of A‖B from the two CRCs and B's
  length without rereading.
- **No dependencies.** One header, standard library only; nothing is fetched.

## 🚀 Quick start

```cpp
#include <crc32c/crc32c.hh>

#include <cstdint>
#include <cstdio>
#include <string_view>

namespace c32 = kronuz::crc32c;

int main() {
	std::string_view header = "header:", body = "the payload bytes";

	std::uint32_t whole = c32::crc32c("header:the payload bytes");    // one shot
	c32::Crc32c crc;                                                   // incremental
	crc.update(header).update(body);
	auto joined = c32::crc32c_combine(c32::crc32c(header), c32::crc32c(body), body.size());
	static_assert(c32::crc32c("123456789") == 0xe3069283);            // constexpr

	std::printf("%08x %08x %08x\n", whole, crc.value(), joined);       // 822f59d0 822f59d0 822f59d0
}
```

[`examples/demo.cc`](examples/demo.cc) also resumes from a stored CRC and prints which path
runs.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer, and nothing else. CI builds it on Linux
(x86_64, arm64), macOS (arm64, x86_64), and FreeBSD, plus ASan, UBSan, and TSan builds, and
requires the hardware path on every runner whose CPU has the instructions.

```cmake
include(FetchContent)
FetchContent_Declare(crc32c
  GIT_REPOSITORY https://github.com/Kronuz/crc32c.git
  GIT_TAG        b60a8e5fcda4cce04c5ea5b6e3eef1645c0da0b8)  # main tracks the tip
FetchContent_MakeAvailable(crc32c)

target_link_libraries(my_app PRIVATE crc32c::crc32c)
```

Then `#include <crc32c/crc32c.hh>`; everything is in `kronuz::crc32c`.

## 🎯 When to use it

It fits storage formats, logs and network protocols that checksum with CRC-32C and want the
hardware instruction without build flags.

Not for:

- **Other CRC-32 polynomials.** It computes Castagnoli's only; zlib's and Ethernet's CRC-32 is
  a different function.
- **Cryptographic integrity.** A CRC detects accidental corruption, not tampering.

## ⚡ Performance

GB/s over one buffer per call, best of two runs of `crc32c_bench` (each the best of 7 trials of
0.2 s); higher is faster. The columns are four ways to compute the same CRC-32C, and the benchmark
checks that the first three return the same value at every size before timing. `crc32c()` is the
library's hardware path. *Google's* is [google/crc32c](https://github.com/google/crc32c) 1.1.2
(`crc32c::Crc32c`), the widely used implementation, fetched and built for the benchmark only.
`loop` is a plain loop of the CPU's own CRC instruction on one dependency chain (`_mm_crc32_u64` or
`__crc32cd`), the code a caller writes without a library. `slice-by-8` is the library's table path.
Every call starts a fresh CRC over the same bytes, so calls overlap in the CPU's pipeline and the
small-buffer rows are throughput, not latency. Apple clang 17, `-O3 -DNDEBUG`.

Apple M4 Pro (macOS 26, arm64):

| Bytes | `crc32c()` | Google's | `loop` | `slice-by-8` |
|---:|---:|---:|---:|---:|
| 64 | 29.7 | 19.5 | 29.6 | 4.9 |
| 128 | 27.9 | 21.2 | 23.3 | 3.8 |
| 512 | 23.7 | 13.3 | 13.3 | 2.7 |
| 1,024 | 27.2 | 11.3 | 11.6 | 2.5 |
| 4,096 | 29.0 | 20.9 | 10.5 | 2.4 |
| 1 MiB | 28.2 | 29.6 | 10.3 | 2.4 |

Intel Core i9-9980HK (macOS 15, x86-64):

| Bytes | `crc32c()` | Google's | `loop` | `slice-by-8` |
|---:|---:|---:|---:|---:|
| 64 | 14.1 | 11.2 | 20.5 | 3.0 |
| 128 | 22.9 | 17.0 | 30.5 | 2.6 |
| 512 | 24.9 | 17.3 | 19.9 | 2.3 |
| 1,024 | 25.8 | 23.5 | 16.8 | 2.2 |
| 4,096 | 29.9 | 30.1 | 12.0 | 2.2 |
| 1 MiB | 29.5 | 30.5 | 10.9 | 2.2 |

Below the smallest stride (96 bytes on the M4 Pro, 384 on the i9) `crc32c()` runs a plain chain like
`loop`; on the i9 it reaches 0.7x to 0.9x of the loop's throughput from 16 to 128 bytes, the
ratio moving with where the compiler places the benchmark's own loop. Above the
stride the lanes run 1.2 to 2.9 times faster than the plain loop on the M4 Pro (from 128 bytes) and
1.25 to 2.8 times on the i9 (from 512 bytes). The library is 1.0x to 2.4x faster than Google's below
4 KiB on both machines and level with it at 4 KiB on the i9. From 64 KiB Google's is level or
ahead: by 8% at 64 KiB and 3% at 1 MiB on the i9, level at 64 KiB and by 5% at 1 MiB on the M4 Pro.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON -DCRC32C_BENCH_REFERENCE=ON
cmake --build build --target crc32c_bench
./build/crc32c_bench
```

[docs/BENCHMARKS.md](docs/BENCHMARKS.md) has every size and the byte table.

## 📚 Documentation

- [docs/API.md](docs/API.md): the full interface.
- [ARCHITECTURE.md](ARCHITECTURE.md): the three paths, the lane merge and combine.
- [docs/BENCHMARKS.md](docs/BENCHMARKS.md): throughput per path.
- [CHANGELOG.md](CHANGELOG.md): changes, newest first.
- [AGENTS.md](AGENTS.md): repository map and rules.
- [Catalog entry](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#crc32c) in Kronuz
  C++ Libraries.

## 🛠️ Building and testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/crc32c_bench
```

The tests check the standard vectors, the hardware routines against the table routine and a
bitwise oracle at every length, alignment and starting state around every lane tier's stride
boundary, every split around the lane and stride boundaries, random fragmentation with empty
pieces, combine, detection before `main`, and a consumer project that links only
`crc32c::crc32c` with fetching disabled. The test prints `crc32c: hardware yes|no`;
`CRC32C_REQUIRE_HARDWARE=1` makes `no` a failure. `crc32c_bench` prints GB/s for the hardware
path, a plain loop of the CPU's CRC instruction, the slice-by-8 table and the byte table.
`-DCRC32C_FUZZ=ON` with Clang builds a libFuzzer target; without it a replay driver runs the
same checks as a test.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo. See [LICENSE](LICENSE).
