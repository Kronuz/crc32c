# crc32c benchmarks

## Throughput

GB/s per call over a buffer of the given size, best of two runs of `bench/crc32c.cc` (each the
best of 7 trials of 0.2 s), Apple clang 17 on macOS, `-O3 -DNDEBUG`; higher is faster.
`crc32c()` is the hardware path: one plain chain below the smallest stride (96 bytes on AArch64, 384
on x86-64), interleaved lanes in tiers of 1 KiB, 256 bytes and 128 bytes (32 on AArch64) above it;
*Google's* is [google/crc32c](https://github.com/google/crc32c) 1.1.2, built with
`-DCRC32C_BENCH_REFERENCE=ON`; `loop` is a plain loop of the CPU's CRC instruction on one
dependency chain (`_mm_crc32_u64` or `__crc32cd`); `slice-by-8` and `byte` are the table paths. The
benchmark checks that the loop's and Google's results equal `crc32c()` at every size.

Apple M4 Pro (macOS 26, arm64):

| Bytes | `crc32c()` | Google's | `loop` | `slice-by-8` | byte |
|---:|---:|---:|---:|---:|---:|
| 16 | 15.2 | 10.0 | 13.3 | 6.1 | 2.0 |
| 64 | 29.7 | 19.5 | 29.6 | 4.9 | 0.97 |
| 128 | 27.9 | 21.2 | 23.3 | 3.8 | 0.72 |
| 256 | 24.9 | 17.6 | 17.6 | 3.0 | 0.60 |
| 512 | 23.7 | 13.3 | 13.3 | 2.7 | 0.56 |
| 1,024 | 27.2 | 11.3 | 11.6 | 2.5 | 0.53 |
| 2,048 | 25.9 | 16.0 | 10.9 | 2.4 | 0.53 |
| 4,096 | 29.0 | 20.9 | 10.5 | 2.4 | 0.52 |
| 65,536 | 29.8 | 29.6 | 10.4 | 2.4 | 0.52 |
| 1 MiB | 28.2 | 29.6 | 10.3 | 2.4 | 0.52 |

Intel Core i9-9980HK (macOS 15, x86-64):

| Bytes | `crc32c()` | Google's | `loop` | `slice-by-8` | byte |
|---:|---:|---:|---:|---:|---:|
| 16 | 3.9 | 3.9 | 5.9 | 3.4 | 1.0 |
| 64 | 14.1 | 11.2 | 20.5 | 3.0 | 0.61 |
| 128 | 22.9 | 17.0 | 30.5 | 2.6 | 0.55 |
| 256 | 23.9 | 19.2 | 25.6 | 2.4 | 0.53 |
| 512 | 24.9 | 17.3 | 19.9 | 2.3 | 0.52 |
| 1,024 | 25.8 | 23.5 | 16.8 | 2.2 | 0.51 |
| 2,048 | 28.7 | 25.4 | 13.3 | 2.2 | 0.51 |
| 4,096 | 29.9 | 30.1 | 12.0 | 2.2 | 0.51 |
| 65,536 | 30.7 | 33.2 | 11.1 | 2.2 | 0.51 |
| 1 MiB | 29.5 | 30.5 | 10.9 | 2.2 | 0.52 |

Every call starts a fresh CRC over the same bytes, so consecutive calls overlap in the CPU's
pipeline; the rows of a few hundred bytes and below are throughput of independent calls, not
the latency of one. At 16 bytes a call is one or two instructions, and the rows of 128 bytes and below move by 10 to
30 percent with where the compiler places the benchmark's own loop, in either direction.

`bench/crc32c.cc` reproduces the tables:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON -DCRC32C_BENCH_REFERENCE=ON
cmake --build build && ./build/crc32c_bench
```

CI runs it on every runner's real CPU (the `hardware-*` jobs) and writes the output to the
job summary, which is where arm64 and the other x86-64 numbers are.
