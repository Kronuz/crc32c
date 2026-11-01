# crc32c benchmarks

## Throughput

GB/s per call over a buffer of the given size, best of three runs of `bench/crc32c.cc` (each
best of 7 trials of 0.2 s), Apple clang 17 on macOS, `-O3 -DNDEBUG`. `crc32c()` is the
hardware path with three interleaved lanes from 3 KiB; `loop` is a plain loop of the CPU's CRC
instruction on one dependency chain (`_mm_crc32_u64` or `__crc32cd`); `slice-by-8` and `byte`
are the table paths. The loop's result is checked against `crc32c()` at every size.

Apple M4 Pro (arm64):

| Bytes | `crc32c()` | loop | slice-by-8 | byte |
|---:|---:|---:|---:|---:|
| 64 | 29.8 | 30.2 | 5.0 | 1.0 |
| 512 | 13.6 | 13.7 | 2.7 | 0.57 |
| 4,096 | 18.1 | 10.9 | 2.5 | 0.53 |
| 65,536 | 22.5 | 10.6 | 2.4 | 0.53 |
| 1 MiB | 22.4 | 10.6 | 2.4 | 0.53 |

Intel Core i9-9980HK (x86_64):

| Bytes | `crc32c()` | loop | slice-by-8 | byte |
|---:|---:|---:|---:|---:|
| 64 | 14.2 | 21.1 | 3.1 | 0.62 |
| 512 | 18.9 | 20.3 | 2.3 | 0.53 |
| 4,096 | 17.9 | 12.3 | 2.2 | 0.52 |
| 65,536 | 21.4 | 11.2 | 2.2 | 0.52 |
| 1 MiB | 20.3 | 11.1 | 2.2 | 0.52 |

Every call starts a fresh CRC over the same bytes, so consecutive calls overlap in the CPU's
pipeline; the 64-byte rows are throughput of independent calls, not the latency of one.

`bench/crc32c.cc` reproduces the tables:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON
cmake --build build && ./build/crc32c_bench
```

CI runs it on every runner's real CPU (the `hardware-*` jobs) and writes the output to the
job summary, which is where arm64 and the other x86-64 numbers are.
