# crc32c benchmarks

## Throughput

GB/s per call over a buffer of the given size, best of three runs of `bench/crc32c.cc` (each
best of 7 trials of 0.2 s), Apple clang 17 on macOS, `-O3 -DNDEBUG`. `crc32c()` is the
hardware path: one plain chain below the smallest stride (96 bytes on AArch64, 384 on x86-64),
interleaved lanes in tiers of 1 KiB, 256 bytes and 128 bytes (32 on AArch64) above it; `loop`
is a plain loop of the CPU's CRC instruction on one dependency chain (`_mm_crc32_u64` or
`__crc32cd`); `slice-by-8` and `byte` are the table paths. The loop's result is checked against
`crc32c()` at every size.

Apple M4 Pro (arm64):

| Bytes | `crc32c()` | loop | slice-by-8 | byte |
|---:|---:|---:|---:|---:|
| 16 | 13.6 | 13.6 | 6.2 | 2.0 |
| 64 | 30.2 | 30.2 | 5.0 | 1.0 |
| 128 | 28.6 | 23.9 | 3.9 | 0.75 |
| 256 | 25.5 | 18.1 | 3.0 | 0.62 |
| 512 | 24.2 | 13.9 | 2.7 | 0.58 |
| 1,024 | 28.0 | 12.1 | 2.5 | 0.56 |
| 2,048 | 26.8 | 11.3 | 2.5 | 0.55 |
| 4,096 | 29.9 | 11.1 | 2.4 | 0.54 |
| 65,536 | 30.4 | 10.8 | 2.4 | 0.54 |
| 1,048,576 | 29.5 | 10.7 | 2.4 | 0.53 |

Intel Core i9-9980HK (x86_64):

| Bytes | `crc32c()` | loop | slice-by-8 | byte |
|---:|---:|---:|---:|---:|
| 16 | 4.0 | 5.7 | 3.6 | 1.0 |
| 64 | 19.4 | 22.5 | 3.1 | 0.63 |
| 128 | 28.6 | 30.8 | 2.6 | 0.56 |
| 256 | 24.1 | 26.0 | 2.4 | 0.54 |
| 512 | 25.5 | 20.4 | 2.3 | 0.53 |
| 1,024 | 25.6 | 17.2 | 2.2 | 0.53 |
| 2,048 | 29.5 | 13.6 | 2.2 | 0.53 |
| 4,096 | 30.4 | 12.2 | 2.2 | 0.52 |
| 65,536 | 31.2 | 11.3 | 2.2 | 0.52 |
| 1,048,576 | 30.1 | 11.2 | 2.2 | 0.52 |

Every call starts a fresh CRC over the same bytes, so consecutive calls overlap in the CPU's
pipeline; the rows of a few hundred bytes and below are throughput of independent calls, not
the latency of one. At 16 bytes a call is one or two instructions and the row moves by 10 to 15
percent with where the compiler places the benchmark's own loop, in either direction.

`bench/crc32c.cc` reproduces the tables:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON
cmake --build build && ./build/crc32c_bench
```

CI runs it on every runner's real CPU (the `hardware-*` jobs) and writes the output to the
job summary, which is where arm64 and the other x86-64 numbers are.
