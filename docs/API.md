# API

Everything is in `<crc32c/crc32c.hh>`, namespace `kronuz::crc32c`. The CRC is the
reflected Castagnoli polynomial (`0x82f63b78`) with initial and final XOR `0xffffffff`,
so `crc32c("123456789") == 0xe3069283` and the CRC of nothing is `0`.

| Name | Meaning |
|---|---|
| `std::uint32_t crc32c(std::string_view)` | `constexpr`, `noexcept`. CRC of the bytes. |
| `std::uint32_t crc32c(const void* data, std::size_t size)` | Runtime, `noexcept`. `size == 0` is fine with a null pointer. |
| `class Crc32c` | Incremental CRC. `Crc32c()` starts at the empty message; `explicit Crc32c(std::uint32_t crc)` resumes from a CRC returned earlier. |
| `Crc32c& update(const void*, std::size_t)` | Append bytes; returns `*this`. Empty updates change nothing. |
| `constexpr Crc32c& update(std::string_view)` | Append bytes; usable at compile time. |
| `constexpr std::uint32_t value() const` | The CRC of everything appended so far; the object stays appendable. |
| `std::uint32_t crc32c_combine(crc_a, crc_b, size_b)` | `constexpr`. CRC of A‖B from `crc32c(A)`, `crc32c(B)` and B's length. |

`kronuz::crc32c::detail` holds the routines (`table_update`, `x86_update`/`arm_update`,
`update`, `hardware_available()`) the tests compare against each other. It is not part
of the interface, but `detail::hardware_available()` is the supported way to ask which
path runs.

## Paths

1. **Hardware.** x86-64 with SSE4.2 (`__builtin_cpu_supports`) and AArch64 with the CRC
   extension (always on Apple; `HWCAP_CRC32` via `getauxval` on Linux; a build with
   `__ARM_FEATURE_CRC32`).
2. **Slice-by-8 table.** Every other CPU, and constant evaluation.

Both give the same value for the same bytes; the choice is invisible except in speed.

## Migrating from hashes

hashes no longer ships `crc32c.hh`. Include `<crc32c/crc32c.hh>` and replace
`hashes::crc32c`, `hashes::Crc32c` and `hashes::crc32c_combine` with the same names in
`kronuz::crc32c`; link `crc32c::crc32c` instead of `hashes::hashes`. The values are the
same.
