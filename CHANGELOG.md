# Changelog

All notable changes to `crc32c`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no releases, so
entries are dated.

## 2026-11-01

### Added

- `crc32c_bench` prints a `loop` column: a plain loop of the CPU's CRC instruction on one
  dependency chain, checked against `crc32c()` at every size. README and
  [docs/BENCHMARKS.md](docs/BENCHMARKS.md) carry the table for an Apple M4 Pro and an Intel
  Core i9-9980HK; on the i9 the plain loop is faster than `crc32c()` at 512 bytes and below
  (21.1 against 14.2 GB/s at 64 bytes).

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
