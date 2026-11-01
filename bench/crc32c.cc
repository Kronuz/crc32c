// CRC-32C throughput: the library's hardware path (three interleaved lanes) against a plain loop
// of the same instruction on one stream, the slice-by-8 table and the byte-at-a-time table.
#include <crc32c/crc32c.hh>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace c32 = kronuz::crc32c;

#if CRC32C_X86 || CRC32C_ARM
#define HAVE_INTRINSIC_LOOP 1
#if CRC32C_X86
#define INTRINSIC_TARGET "sse4.2"
#define INTRINSIC_STEP8(c, v) _mm_crc32_u64((c), (v))
#define INTRINSIC_STEP1(c, b) _mm_crc32_u8((c), (b))
#else
#define INTRINSIC_TARGET "+crc"
#define INTRINSIC_STEP8(c, v) std::uint64_t(__crc32cd(std::uint32_t(c), (v)))
#define INTRINSIC_STEP1(c, b) __crc32cb((c), (b))
#endif

// What a caller writes without the library: one dependency chain of the CPU's CRC instruction.
__attribute__((target(INTRINSIC_TARGET))) static std::uint32_t intrinsic_loop(const unsigned char* p, std::size_t n) {
	std::uint64_t crc = ~0u;
	for (; n >= 8; p += 8, n -= 8) {
		std::uint64_t v;
		std::memcpy(&v, p, 8);
		crc = INTRINSIC_STEP8(crc, v);
	}
	std::uint32_t c = static_cast<std::uint32_t>(crc);
	for (; n; ++p, --n) { c = INTRINSIC_STEP1(c, *p); }
	return ~c;
}
#endif

template <typename F>
static double gb_per_s(F&& f, std::size_t bytes) {
	using clock = std::chrono::steady_clock;
	volatile std::uint32_t sink = 0;
	double best = 0;
	for (int trial = 0; trial < 7; ++trial) {
		std::size_t total = 0;
		auto start = clock::now();
		while (std::chrono::duration<double>(clock::now() - start).count() < 0.2) {
			for (int i = 0; i < 256; ++i) { sink = sink + f(); }
			total += 256 * bytes;
		}
		double seconds = std::chrono::duration<double>(clock::now() - start).count();
		best = std::max(best, total / seconds / 1e9);
	}
	return best;
}

int main() {
	std::mt19937_64 rng(1);
	std::vector<unsigned char> data(1 << 20);
	for (auto& b : data) { b = static_cast<unsigned char>(rng()); }
#ifdef HAVE_INTRINSIC_LOOP
	const bool loop_available = c32::detail::hardware_available();
#else
	const bool loop_available = false;
#endif
	std::printf("%-10s %12s %12s %12s %12s\n", "bytes", "hw GB/s", "loop GB/s", "slice8 GB/s", "byte GB/s");
	for (std::size_t n : {64u, 512u, 4096u, 65536u, 1u << 20}) {
		auto hw = gb_per_s([&] { return c32::crc32c(data.data(), n); }, n);
		double lp = 0;
#ifdef HAVE_INTRINSIC_LOOP
		if (loop_available) {
			if (intrinsic_loop(data.data(), n) != c32::crc32c(data.data(), n)) {
				std::fprintf(stderr, "the instruction loop disagrees with the library at %zu bytes\n", std::size_t{n});
				return 1;
			}
			lp = gb_per_s([&] { return intrinsic_loop(data.data(), n); }, n);
		}
#endif
		auto s8 = gb_per_s([&] { return ~c32::detail::table_update(~0u, data.data(), n); }, n);
		auto b1 = gb_per_s([&] {
			std::uint32_t crc = ~0u;
			for (std::size_t i = 0; i < n; ++i) { crc = c32::detail::tables[0][(crc ^ data[i]) & 0xff] ^ (crc >> 8); }
			return ~crc;
		}, n);
		if (loop_available) {
			std::printf("%-10zu %12.2f %12.2f %12.2f %12.2f\n", n, hw, lp, s8, b1);
		} else {
			std::printf("%-10zu %12.2f %12s %12.2f %12.2f\n", n, hw, "n/a", s8, b1);
		}
	}
}
