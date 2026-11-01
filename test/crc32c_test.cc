// CRC-32C: known vectors, hardware against the table path, incremental and combine.
#include <crc32c/crc32c.hh>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace c32 = kronuz::crc32c;

static int failures = 0;
#define CHECK(cond) \
	do { \
		if (!(cond)) { std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); ++failures; } \
	} while (0)

static std::uint32_t reference(const unsigned char* p, std::size_t n) {
	return ~c32::detail::table_update(~0u, p, n);
}

static std::uint32_t bitwise(const unsigned char* p, std::size_t n) {
	std::uint32_t crc = ~0u;
	for (std::size_t i = 0; i < n; ++i) {
		crc ^= p[i];
		for (int bit = 0; bit < 8; ++bit) { crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78u : 0); }
	}
	return ~crc;
}

// The bitwise definition from an arbitrary raw state: no table, no instruction, independent of the library's paths.
static std::uint32_t bitwise_from(std::uint32_t crc, const unsigned char* p, std::size_t n) {
	for (std::size_t i = 0; i < n; ++i) {
		crc ^= p[i];
		for (int bit = 0; bit < 8; ++bit) { crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78u : 0); }
	}
	return crc;
}

// The CPU's instructions as seen by a program that checksums while its namespace-scope objects are constructed. Some
// toolchains fill in the CPU model in a constructor that has not run by then, and a detection cached too early would
// keep the table path for the whole run.
[[maybe_unused]] static const bool detected_before_main = kronuz::crc32c::detail::hardware_available();

int main() {
	// RFC 3720 (iSCSI) and the common check value.
	static_assert(c32::crc32c("123456789") == 0xe3069283u);
	static_assert(c32::crc32c("") == 0u);
	CHECK(c32::crc32c(std::string_view("123456789")) == 0xe3069283u);
	std::vector<unsigned char> zeros(32, 0), ones(32, 0xff), inc(32), dec(32);
	for (int i = 0; i < 32; ++i) { inc[i] = static_cast<unsigned char>(i); dec[i] = static_cast<unsigned char>(31 - i); }
	CHECK(c32::crc32c(zeros.data(), 32) == 0x8a9136aau);
	CHECK(c32::crc32c(ones.data(), 32) == 0x62a8ab43u);
	CHECK(c32::crc32c(inc.data(), 32) == 0x46dd794eu);
	CHECK(c32::crc32c(dec.data(), 32) == 0x113fdb5cu);

	std::mt19937_64 rng(20261011);
	std::vector<unsigned char> buffer(20000 + 16);
	for (auto& b : buffer) { b = static_cast<unsigned char>(rng()); }

	// Every length across the 3-lane threshold, at every alignment.
	for (std::size_t offset = 0; offset < 8; ++offset) {
		for (std::size_t n = 0; n <= 3 * 1024 * 2 + 64; n += (n < 256 ? 1 : 7)) {
			auto expected = reference(buffer.data() + offset, n);
			CHECK(c32::crc32c(buffer.data() + offset, n) == expected);
		}
	}
	CHECK(reference(buffer.data(), 1000) == bitwise(buffer.data(), 1000));
	CHECK(c32::crc32c(buffer.data(), 20000) == bitwise(buffer.data(), 20000));

	// Incremental updates in random pieces equal the one-shot value.
	for (int round = 0; round < 200; ++round) {
		std::size_t n = rng() % buffer.size();
		c32::Crc32c crc;
		for (std::size_t at = 0; at < n;) {
			std::size_t piece = std::min<std::size_t>(n - at, rng() % 4000);
			crc.update(buffer.data() + at, piece);
			at += piece;
		}
		CHECK(crc.value() == c32::crc32c(buffer.data(), n));
	}

	// Empty input: the CRC of nothing is 0, and empty updates change nothing, whatever the state.
	static_assert(c32::Crc32c{}.update(std::string_view{}).value() == 0u);
	CHECK(c32::crc32c(nullptr, 0) == 0u);
	CHECK(c32::Crc32c{}.update(nullptr, 0).value() == 0u);
	CHECK(c32::Crc32c{}.update(std::string_view{}).value() == 0u);
	for (int round = 0; round < 50; ++round) {
		auto seed = static_cast<std::uint32_t>(rng());
		CHECK(c32::Crc32c(seed).update(std::string_view{}).update(nullptr, 0).value() == seed);
		CHECK(c32::Crc32c(seed).update(buffer.data(), 0).value() == seed);
	}

	// Every two-way split of lengths around the lane and stride boundaries, including empty halves.
	for (std::size_t n : {0, 1, 7, 8, 9, 63, 1023, 1024, 1025, 2047, 3071, 3072, 3073, 3100}) {
		auto expected = c32::crc32c(buffer.data(), n);
		for (std::size_t cut = 0; cut <= n; ++cut) {
			c32::Crc32c crc;
			crc.update(buffer.data(), cut).update(buffer.data() + cut, n - cut);
			CHECK(crc.value() == expected);
		}
	}

	// Pieces of random size (often empty), through both overloads, and one byte at a time.
	for (int round = 0; round < 300; ++round) {
		std::size_t n = rng() % buffer.size();
		auto expected = reference(buffer.data(), n);
		c32::Crc32c by_pointer, by_view;
		for (std::size_t at = 0; at < n;) {
			std::size_t piece = std::min<std::size_t>(n - at, rng() % (round % 3 ? 5000 : 3));
			by_pointer.update(buffer.data() + at, piece);
			by_view.update(std::string_view(reinterpret_cast<const char*>(buffer.data()) + at, piece));
			at += piece;
		}
		CHECK(by_pointer.value() == expected);
		CHECK(by_view.value() == expected);
	}
	{
		c32::Crc32c crc;
		for (std::size_t i = 0; i < 5000; ++i) { crc.update(buffer.data() + i, 1); }
		CHECK(crc.value() == reference(buffer.data(), 5000));
	}

#if CRC32C_X86 || CRC32C_ARM
	// The hardware routine against the table routine from arbitrary states, lengths and alignments.
	if (c32::detail::hardware_available()) {
		for (int round = 0; round < 2000; ++round) {
			auto state = static_cast<std::uint32_t>(rng());
			std::size_t offset = rng() % 16, n = rng() % (round % 10 ? 1500 : 12000);
			auto expected = c32::detail::table_update(state, buffer.data() + offset, n);
			CHECK(c32::detail::hardware_update(state, buffer.data() + offset, n) == expected);
			CHECK(c32::detail::update(state, buffer.data() + offset, n) == expected);
		}
	}

	// Below `smallest_stride` a buffer takes the plain loop, above it the interleaved path, one tier of lanes after
	// another. Every hardware entry point against the bitwise oracle: random lengths 0 to 4096 at random alignments,
	// then every length within 9 bytes of each tier's stride boundary and of each sum of whole strides of the three
	// tiers (the last length below a boundary, the first at it, the byte after each lane merge, the tail steps), from
	// random and extreme starting states. Each buffer is an exact-size heap block, so a read past its end is an
	// AddressSanitizer error.
	if (c32::detail::hardware_available()) {
		auto check_hardware = [&](std::uint32_t state, std::size_t offset, std::size_t n) {
			std::unique_ptr<unsigned char[]> block(new unsigned char[offset + n]);
			std::memcpy(block.get(), buffer.data(), offset + n);
			const unsigned char* p = block.get() + offset;
			auto expected = bitwise_from(state, p, n);
			CHECK(c32::detail::hardware_plain(state, p, n) == expected);
			CHECK(c32::detail::hardware_update(state, p, n) == expected);
			CHECK(c32::detail::update(state, p, n) == expected);
			CHECK(c32::Crc32c(~state).update(p, n).value() == ~expected);
		};
		for (int round = 0; round < 3000; ++round) {
			check_hardware(static_cast<std::uint32_t>(rng()), rng() % 16, rng() % 4097);
		}
		// Whole strides of each tier alone (every sum of strides below includes a, b or c of zero), then all three.
		const std::size_t large = 3 * c32::detail::lane_large, middle = 3 * c32::detail::lane_middle, least = 3 * c32::detail::lane_small;
		const std::uint32_t states[] = {0u, ~0u, 0x80000000u, static_cast<std::uint32_t>(rng())};
		for (std::size_t a = 0; a <= 2; ++a) {
			for (std::size_t b = 0; b <= 3; ++b) {
				for (std::size_t c = 0; c <= 4; ++c) {
					std::size_t edge = a * large + b * middle + c * least;
					for (std::size_t n = edge > 9 ? edge - 9 : 0; n <= edge + 9; ++n) {
						for (std::size_t offset : {std::size_t{0}, std::size_t{3}, std::size_t{15}}) {
							for (auto state : states) { check_hardware(state, offset, n); }
						}
					}
				}
			}
		}
	}
#endif

	// Combine and resume.
	for (int round = 0; round < 200; ++round) {
		std::size_t a = rng() % 10000, b = rng() % 10000;
		auto ca = c32::crc32c(buffer.data(), a);
		auto cb = c32::crc32c(buffer.data() + a, b);
		CHECK(c32::crc32c_combine(ca, cb, b) == c32::crc32c(buffer.data(), a + b));
		CHECK(c32::Crc32c(ca).update(buffer.data() + a, b).value() == c32::crc32c(buffer.data(), a + b));
	}

#if CRC32C_X86
	// The detection that ran before main agrees with the CPU asked now.
	__builtin_cpu_init();
	CHECK(detected_before_main == static_cast<bool>(__builtin_cpu_supports("sse4.2")));
#endif

	// CI sets this on runners whose CPU has the instructions, so a test that quietly ran only the table fails.
	if (std::getenv("CRC32C_REQUIRE_HARDWARE") && !c32::detail::hardware_available()) {
		std::fprintf(stderr, "CRC32C_REQUIRE_HARDWARE is set but no hardware path is available\n");
		++failures;
	}

	std::printf("crc32c: hardware %s, %s\n", c32::detail::hardware_available() ? "yes" : "no",
		failures ? "FAILED" : "ok");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
