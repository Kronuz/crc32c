# Changelog

All notable changes to `crc32c`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no releases, so
entries are dated.

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
