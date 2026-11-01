/*
 * Copyright (c) 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// CRC-32C (Castagnoli, reflected, init and final XOR 0xffffffff), as used by
// iSCSI, ext4, and kronuz::journal. Hardware instructions on x86 (SSE4.2) and
// ARMv8 (CRC32), chosen at runtime; a slice-by-8 table everywhere else and at
// compile time.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#if defined(__x86_64__) || defined(_M_X64)
#include <nmmintrin.h>
#define CRC32C_X86 1
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
#include <arm_acle.h>
#define CRC32C_ARM 1
#if defined(__linux__) && !defined(__ARM_FEATURE_CRC32)
#include <sys/auxv.h>
#endif
#endif

namespace kronuz::crc32c {

namespace detail {

inline constexpr std::uint32_t poly = 0x82f63b78u;  // reflected 0x1edc6f41

inline constexpr auto tables = [] {
	std::array<std::array<std::uint32_t, 256>, 8> t{};
	for (std::uint32_t i = 0; i < 256; ++i) {
		auto v = i;
		for (int bit = 0; bit < 8; ++bit) { v = (v >> 1) ^ ((v & 1) ? poly : 0); }
		t[0][i] = v;
	}
	for (std::size_t k = 1; k < 8; ++k) {
		for (std::size_t i = 0; i < 256; ++i) { t[k][i] = (t[k - 1][i] >> 8) ^ t[0][t[k - 1][i] & 0xff]; }
	}
	return t;
}();

// Advances a raw (non-inverted) CRC state over bytes, eight at a time.
constexpr std::uint32_t table_update(std::uint32_t crc, const unsigned char* p, std::size_t n) noexcept {
	for (; n >= 8; p += 8, n -= 8) {
		std::uint32_t lo = crc ^ (std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24);
		crc = tables[7][lo & 0xff] ^ tables[6][(lo >> 8) & 0xff] ^ tables[5][(lo >> 16) & 0xff] ^ tables[4][lo >> 24] ^
		      tables[3][p[4]] ^ tables[2][p[5]] ^ tables[1][p[6]] ^ tables[0][p[7]];
	}
	for (; n; ++p, --n) { crc = tables[0][(crc ^ *p) & 0xff] ^ (crc >> 8); }
	return crc;
}

// a * b modulo the CRC polynomial, in the reflected bit order (bit 31 is x^0).
constexpr std::uint32_t multiply(std::uint32_t a, std::uint32_t b) noexcept {
	std::uint32_t product = 0;
	for (std::uint32_t m = 1u << 31; m; m >>= 1) {
		if (a & m) { product ^= b; }
		b = (b & 1) ? (b >> 1) ^ poly : b >> 1;
	}
	return product;
}

// x^(8 * bytes) modulo the polynomial: the factor that shifts a CRC state past `bytes` zero bytes.
constexpr std::uint32_t shift_factor(std::uint64_t bytes) noexcept {
	std::uint32_t result = 1u << 31, square = 1u << 23;  // x^0 and x^8
	for (; bytes; bytes >>= 1) {
		if (bytes & 1) { result = multiply(result, square); }
		square = multiply(square, square);
	}
	return result;
}

// a * K modulo the polynomial for a compile-time K, through four byte-indexed tables (4 KiB): the product is four loads,
// where multiply() above is a 32-step loop.
template <std::uint32_t K>
struct shift_table {
	std::uint32_t t[4][256]{};
	constexpr shift_table() noexcept {
		for (std::size_t i = 0; i < 4; ++i) {
			std::uint32_t basis[8];
			for (std::size_t k = 0; k < 8; ++k) { basis[k] = multiply(std::uint32_t(1) << (8 * i + k), K); }
			for (std::size_t b = 1; b < 256; ++b) {
				std::size_t low = 0;
				while (!((b >> low) & 1)) { ++low; }
				t[i][b] = t[i][b & (b - 1)] ^ basis[low];
			}
		}
	}
	constexpr std::uint32_t operator()(std::uint32_t a) const noexcept {
		return t[0][a & 0xff] ^ t[1][(a >> 8) & 0xff] ^ t[2][(a >> 16) & 0xff] ^ t[3][a >> 24];
	}
};
template <std::uint32_t K> inline constexpr shift_table<K> shift_by{};

inline std::uint64_t load64(const unsigned char* p) noexcept {
	std::uint64_t v;
	std::memcpy(&v, p, 8);
	return v;
}

#if CRC32C_X86 || CRC32C_ARM
#if CRC32C_X86
#define CRC32C_TARGET "sse4.2"
#define CRC32C_STEP8(c, v) _mm_crc32_u64((c), (v))
#define CRC32C_STEP1(c, b) _mm_crc32_u8((c), (b))
#else
#define CRC32C_TARGET "+crc"
#define CRC32C_STEP8(c, v) std::uint64_t(__crc32cd(std::uint32_t(c), (v)))
#define CRC32C_STEP1(c, b) __crc32cb((c), (b))
#endif

// The lane sizes of the interleaved path, largest first; a stride is three lanes of one size, and three interleaved
// streams hide the 3-cycle latency of the CRC instruction. A buffer shorter than the smallest stride takes the plain
// loop. The smallest lane is the shortest at which a stride beats that loop in the benchmark: 128 bytes on x86-64,
// 32 on AArch64.
#if CRC32C_X86
inline constexpr std::size_t lane_large = 1024, lane_middle = 256, lane_small = 128;
#else
inline constexpr std::size_t lane_large = 1024, lane_middle = 256, lane_small = 32;
#endif
inline constexpr std::size_t smallest_stride = 3 * lane_small;

// One dependency chain of 8-byte steps, then bytes. A leaf with no frame: the path of small buffers.
__attribute__((target(CRC32C_TARGET))) inline std::uint32_t hardware_plain(std::uint32_t crc, const unsigned char* p, std::size_t n) noexcept {
	std::uint64_t v = crc;
	for (; n >= 8; p += 8, n -= 8) { v = CRC32C_STEP8(v, load64(p)); }
	crc = std::uint32_t(v);
	for (; n; ++p, --n) { crc = CRC32C_STEP1(crc, *p); }
	return crc;
}

// Whole strides of three `Lane`-byte lanes, merged with shift tables; advances the state, pointer and count.
template <std::size_t Lane>
__attribute__((target(CRC32C_TARGET), always_inline)) inline void hardware_strides(std::uint32_t& crc, const unsigned char*& p, std::size_t& n) noexcept {
	constexpr auto& shift_one = shift_by<shift_factor(Lane)>;
	constexpr auto& shift_two = shift_by<shift_factor(2 * Lane)>;
	for (; n >= 3 * Lane; p += 3 * Lane, n -= 3 * Lane) {
		std::uint64_t a = crc, b = 0, c = 0;
		for (std::size_t i = 0; i < Lane; i += 8) {
			a = CRC32C_STEP8(a, load64(p + i));
			b = CRC32C_STEP8(b, load64(p + Lane + i));
			c = CRC32C_STEP8(c, load64(p + 2 * Lane + i));
		}
		crc = shift_two(std::uint32_t(a)) ^ shift_one(std::uint32_t(b)) ^ std::uint32_t(c);
	}
}

// Any length: strides of the large lanes, then the middle ones, then the small ones, then the plain loop on the rest.
__attribute__((target(CRC32C_TARGET), noinline)) inline std::uint32_t hardware_update(std::uint32_t crc, const unsigned char* p, std::size_t n) noexcept {
	hardware_strides<lane_large>(crc, p, n);
	hardware_strides<lane_middle>(crc, p, n);
	hardware_strides<lane_small>(crc, p, n);
	return hardware_plain(crc, p, n);
}

#undef CRC32C_TARGET
#undef CRC32C_STEP8
#undef CRC32C_STEP1
#endif

// Whether the CPU has the instructions. The question is asked once and the answer kept in a constant-initialized
// byte (0 not asked, 1 table, 2 hardware), so a call made at any point of start-up is safe and every later one is a
// single relaxed load. Threads that ask at the same time get the same answer.
inline bool cpu_has_hardware() noexcept {
#if CRC32C_X86
	__builtin_cpu_init();  // the CPU model is otherwise filled in by a constructor that may not have run yet
	return __builtin_cpu_supports("sse4.2");
#elif CRC32C_ARM && defined(__linux__)
	return (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0;
#else
	return false;
#endif
}

inline std::atomic<std::uint8_t> hardware_state{0};

__attribute__((noinline)) inline bool ask_hardware() noexcept {
	std::uint8_t state = cpu_has_hardware() ? 2 : 1;
	hardware_state.store(state, std::memory_order_relaxed);
	return state == 2;
}

inline bool hardware_available() noexcept {
#if CRC32C_ARM && (defined(__ARM_FEATURE_CRC32) || defined(__APPLE__))
	return true;
#elif CRC32C_X86 || (CRC32C_ARM && defined(__linux__))
	std::uint8_t state = hardware_state.load(std::memory_order_relaxed);
	return state ? state == 2 : ask_hardware();
#else
	return false;
#endif
}

inline std::uint32_t update(std::uint32_t crc, const unsigned char* p, std::size_t n) noexcept {
#if CRC32C_X86 || CRC32C_ARM
	if (hardware_available()) { return n < smallest_stride ? hardware_plain(crc, p, n) : hardware_update(crc, p, n); }
#endif
	return table_update(crc, p, n);
}

}  // namespace detail

// Incremental CRC-32C: feed bytes in any number of pieces, then read value().
class Crc32c {
public:
	constexpr Crc32c() noexcept = default;
	constexpr explicit Crc32c(std::uint32_t value) noexcept : state_(~value) {}

	Crc32c& update(const void* data, std::size_t size) noexcept {
		state_ = detail::update(state_, static_cast<const unsigned char*>(data), size);
		return *this;
	}
	constexpr Crc32c& update(std::string_view bytes) noexcept {
		if (std::is_constant_evaluated()) {
			for (char ch : bytes) { state_ = detail::tables[0][(state_ ^ static_cast<unsigned char>(ch)) & 0xff] ^ (state_ >> 8); }
			return *this;
		}
		return update(bytes.data(), bytes.size());
	}
	constexpr std::uint32_t value() const noexcept { return ~state_; }

private:
	std::uint32_t state_ = ~0u;
};

constexpr std::uint32_t crc32c(std::string_view bytes) noexcept { return Crc32c{}.update(bytes).value(); }

inline std::uint32_t crc32c(const void* data, std::size_t size) noexcept { return Crc32c{}.update(data, size).value(); }

// CRC of A‖B from crc32c(A), crc32c(B), and B's length, without rereading the bytes.
constexpr std::uint32_t crc32c_combine(std::uint32_t crc_a, std::uint32_t crc_b, std::uint64_t size_b) noexcept {
	// Raw states: ~crc. State after A‖B = shift(stateA, |B|) ^ raw_crc(B from 0); raw_crc(B from 0) = stateB ^ shift(~0, |B|).
	auto shift = detail::shift_factor(size_b);
	return ~(detail::multiply(~crc_a, shift) ^ ~crc_b ^ detail::multiply(~0u, shift));
}

}  // namespace kronuz::crc32c
