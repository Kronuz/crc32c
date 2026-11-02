# Changelog

All notable changes to `crc32c`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no releases, so
entries are dated.

## 2026-11-01

### Changed

- `crc32c()` reaches the hardware routine without an out-of-line `detail::update()` in between.
  The table loop is now `detail::table_update_out_of_line()`, so `update()` is a state check, a
  length test and one call, and inlines into the caller. Before, Apple clang on x86-64 kept
  `update()` out of line behind a frame of four saved registers (the inlined table loop wanted
  them), and every short buffer paid a second call before the routine started. The test
  `crc32c_codegen` compiles `test/codegen_probe.cc` and requires that `crc32c()` does not refer
  to `update()`; it fails on the header of the commit before this one on Apple clang for x86-64.
  GB/s, best of three interleaved rounds of `crc32c_bench` (0.08 s trials), i9-9980HK, START
  (`b60a8e5`), before and after: 16 B 3.76, 3.94, 4.45; 64 B 13.75, 19.24, 19.10; 128 B 22.99,
  28.13, 27.43; 4 KiB 17.78, 30.01, 29.92; 1 MiB 19.90, 30.91, 29.80. No row of the benchmark is
  more than 2 percent below the commit before (median of the paired rounds). The M4 Pro is
  unchanged (16 B: 13.58, 13.46, 13.50). A short-buffer path of inline assembly (the CRC
  instruction with no call) was measured and is not in the library: 6.9 GB/s at 16 bytes, but 13
  at 64 bytes and 14 at 128 against 19 and 27 for the call.
- The hardware path runs in tiers: strides of three lanes of 1 KiB, then 256 bytes, then 128
  bytes (32 on AArch64), and a plain chain of the instruction below the smallest stride (384
  bytes on x86-64, 96 on AArch64) and for what the strides leave. The lanes merge through
  byte-indexed tables built at compile time (4 KiB per shift) instead of a 32-step loop, which is
  what lets a stride pay at a few hundred bytes. Before, buffers under 3 KiB ran one chain inside
  the routine that also held the 3 KiB loop. Values and the public interface are unchanged;
  `detail::x86_update` and `detail::arm_update` are now `detail::hardware_update` and
  `detail::hardware_plain`. GB/s, best of three runs of `crc32c_bench`, before and after, on an
  Apple M4 Pro and an Intel Core i9-9980HK (Apple clang 17, `-O3`):

  | Bytes | M4 Pro before | after | i9 before | after |
  |---:|---:|---:|---:|---:|
  | 16 | 15.6 | 13.6 | 3.8 | 4.0 |
  | 64 | 30.4 | 30.2 | 13.7 | 19.4 |
  | 128 | 23.9 | 28.7 | 23.4 | 28.6 |
  | 256 | 18.1 | 25.5 | 23.5 | 24.1 |
  | 512 | 13.9 | 24.2 | 18.8 | 25.5 |
  | 1,024 | 12.1 | 28.0 | 16.8 | 25.6 |
  | 2,048 | 11.4 | 26.8 | 13.5 | 29.5 |
  | 4,096 | 18.4 | 29.9 | 18.2 | 30.4 |
  | 65,536 | 22.6 | 30.4 | 21.3 | 31.2 |
  | 1,048,576 | 22.4 | 29.5 | 20.4 | 30.1 |

  At 64 bytes on the i9 the loop column measures 22.5, so the call runs at 86 percent of the plain
  loop (before: 66 percent). The 16-byte row on the M4 Pro reads 15.6 before and 13.6 after in
  the full benchmark and 13.6 before and 15.5 after in a benchmark of the small rows only: the
  row follows the placement of the benchmark's loop, not the library.

### Fixed

- A CRC taken while a program's namespace-scope objects are constructed could keep the library
  on the slice-by-8 table for the whole run. On macOS the CPU model that `__builtin_cpu_supports`
  reads is filled in by a constructor that runs after the program's own, and the answer was
  cached at the first call. Detection now calls `__builtin_cpu_init()` first and keeps its answer
  in a constant-initialized byte, so it is safe at any point of start-up and costs one relaxed
  load per call. The values were right all along; the cost was 10 times the throughput.

### Added

- `bench/crc32c.cc` times [Google's crc32c](https://github.com/google/crc32c) 1.1.2 beside the
  library, built only with `-DCRC32C_BENCH_REFERENCE=ON` in a project of its own, and checks
  its result against the library's at every size before timing. CMake 4 builds it (the minimum
  version its own CMakeLists declares is raised for the reference build only).
- Tests of every hardware entry point against a bitwise oracle (no table, no instruction) at
  random lengths 0 to 4096 and alignments, and at every length within 9 bytes of each tier's
  stride boundary and of each sum of whole strides, from extreme and random starting states,
  on exact-size heap blocks; a test of detection before `main`; the fuzz target checks the
  hardware routines directly. `crc32c_bench` prints the rows 16, 128, 256, 1,024 and 2,048 as
  well.
- `crc32c_bench` prints a `loop` column: a plain loop of the CPU's CRC instruction on one
  dependency chain, checked against `crc32c()` at every size. README and
  [docs/BENCHMARKS.md](docs/BENCHMARKS.md) carry the table for an Apple M4 Pro and an Intel
  Core i9-9980HK; on the i9 the plain loop was faster than `crc32c()` at 512 bytes and below
  (21.1 against 14.2 GB/s at 64 bytes) before the tiers above.

## 2026-10-22

### Added

- CRC-32C as its own library. It lived in [hashes](https://github.com/Kronuz/hashes)
  (`crc32c.hh`, namespace `hashes`) and as a byte table in
  [journal](https://github.com/Kronuz/journal); this is the one implementation, and its
  earlier history is the file's history in hashes. To migrate from hashes, include
  `<crc32c/crc32c.hh>` and replace `hashes::` with `kronuz::crc32c::` (`crc32c`, `Crc32c`,
  `crc32c_combine`); the values are the same.
- [docs/BENCHMARKS.md](docs/BENCHMARKS.md) carries the throughput table measured in hashes
  before the move.
