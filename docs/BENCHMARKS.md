# crc32c benchmarks

## Throughput

Moved from hashes' benchmarks, measured there on an Intel i9-9980HK (macOS, Apple clang,
`-O2`; measured while the host ran other builds, so treat the numbers as indicative):

| Bytes | Hardware | Slice-by-8 | Byte table |
|---:|---:|---:|---:|
| 64 | 14.3 GB/s | 3.0 GB/s | 0.60 GB/s |
| 4,096 | 16.7 GB/s | 2.1 GB/s | 0.49 GB/s |
| 1 MiB | 19.3 GB/s | 2.1 GB/s | 0.49 GB/s |

`bench/crc32c.cc` reproduces the table:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON
cmake --build build && ./build/crc32c_bench
```

CI runs it on every runner's real CPU (the `hardware-*` jobs) and writes the output to the
job summary, which is where arm64 and the other x86-64 numbers are.
