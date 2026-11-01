# crc32c

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/crc32c/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/crc32c/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

CRC-32C for C++20: SSE4.2 and ARMv8 instructions picked at run time, slice-by-8 elsewhere,
constexpr and combine. The hardware routines run three interleaved lanes and are compiled with
a function-level target attribute, so no global `-msse4.2` or `-march` flag is needed; a
slice-by-8 table covers other CPUs and compile time. CRC-32C (Castagnoli) is the checksum of
iSCSI, ext4, Btrfs and [journal](https://github.com/Kronuz/journal); reach for this one header
when you need it fast and dependency-free.

## ✨ Highlights

- **Hardware where the CPU has it.** `_mm_crc32_u64` on x86-64 and `__crc32cd` on AArch64,
  detected once at run time.
- **Three interleaved lanes.** Buffers of 3 KiB and more run three 1 KiB lanes at once to hide
  the instruction's latency; shift constants computed at compile time merge the lanes.
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

GB/s over one buffer per call, best of three runs of the benchmark (each best of 7 trials of
0.2 s). `crc32c()` is the library's hardware path; `loop` is a plain loop of the CPU's own CRC
instruction on one dependency chain (`_mm_crc32_u64` or `__crc32cd`), the code a caller writes
without the library; `slice-by-8` is the library's table path. Every call starts a fresh CRC
over the same bytes, so calls overlap in the CPU's pipeline and the small-buffer rows are
throughput, not latency.

| Bytes | M4 Pro: `crc32c()` | loop | slice-by-8 | i9: `crc32c()` | loop | slice-by-8 |
|---:|---:|---:|---:|---:|---:|---:|
| 64 | 29.8 | 30.2 | 5.0 | 14.2 | 21.1 | 3.1 |
| 512 | 13.6 | 13.7 | 2.7 | 18.9 | 20.3 | 2.3 |
| 4,096 | 18.1 | 10.9 | 2.5 | 17.9 | 12.3 | 2.2 |
| 1 MiB | 22.4 | 10.6 | 2.4 | 20.3 | 11.1 | 2.2 |

From 4 KiB the three interleaved lanes run 1.5 to 2.1 times faster than the plain loop. Below
that `crc32c()` runs the same single chain, and on the i9 the plain loop is faster than it
(21.1 against 14.2 GB/s at 64 bytes); on the M4 Pro the two are equal. Apple clang 17 on
macOS, `-O3 -DNDEBUG`, on an Apple M4 Pro (arm64) and an Intel Core i9-9980HK (x86_64):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON
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

The tests check the standard vectors, the hardware routine against the table routine at every
length, alignment and starting state, every split around the lane and stride boundaries,
random fragmentation with empty pieces, combine, and a consumer project that links only
`crc32c::crc32c` with fetching disabled. The test prints `crc32c: hardware yes|no`;
`CRC32C_REQUIRE_HARDWARE=1` makes `no` a failure. `crc32c_bench` prints GB/s for the hardware path, a plain loop of the CPU's CRC instruction, the
slice-by-8 table and the byte table. `-DCRC32C_FUZZ=ON` with Clang builds a libFuzzer target;
without it a replay driver runs the same checks as a test.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo. See [LICENSE](LICENSE).
