# Architecture

`include/crc32c/crc32c.hh` is the whole library. It includes only the standard library
and the platform's intrinsics header, and defines three layers.

```
  crc32c(), Crc32c, crc32c_combine      public, constexpr where the table path allows
        |
  detail::update(state, p, n)           picks hardware or table, once-detected
      /                 \
  x86_update / arm_update   table_update     (hardware in target-attributed functions)
```

## The paths

The reflected Castagnoli polynomial (`0x82f63b78`) drives both.

1. **Hardware.** `_mm_crc32_u64` on x86-64 and `__crc32cd` on AArch64, in functions
   compiled with the target attribute (`sse4.2`, `+crc`), so the library needs no
   global `-msse4.2` or `-march` flag and a portable binary still runs on a CPU
   without the instructions. Detection runs once and is cached
   (`__builtin_cpu_supports`, `getauxval(AT_HWCAP)`, always-on on Apple Silicon).
2. **Three lanes.** The CRC instruction has 3-cycle latency and 1-cycle throughput, so
   one dependent chain uses a third of the unit. Buffers of 3 KiB and more are split
   into 3 × 1 KiB lanes computed together; lane states merge with
   `multiply(state, x^(8·bytes))` modulo the polynomial, and the two shift factors are
   computed at compile time.
3. **Table.** Slice-by-8, built at compile time, for CPUs without the instruction and
   for constant evaluation (`Crc32c::update(string_view)` takes the byte-table route
   inside `std::is_constant_evaluated()`).

`crc32c_combine` uses the same shift arithmetic: the state after A‖B is
`shift(stateA, |B|) ^ raw_crc(B from 0)`, and `raw_crc(B from 0)` is recovered from
`crc32c(B)` and `shift(~0, |B|)`.

## State

`Crc32c` stores the raw (inverted) register: construction from a CRC `v` stores `~v`,
`value()` returns `~state`. That makes resume-from-stored-CRC and empty updates exact:
an empty update returns the register untouched.

## Seams

There are none to inject: no I/O, no allocation, no logging, no global state beyond the
cached hardware flag. The consumer-visible choices are the compiler's, not the library's.
