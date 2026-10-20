// libFuzzer target: any bytes, cut into pieces by the input itself, must give the same CRC as
// the one-shot value, the slice-by-8 table and the bitwise reference, from any starting state.
#include <crc32c/crc32c.hh>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace c32 = kronuz::crc32c;

static std::uint32_t bitwise(std::uint32_t crc, const unsigned char* p, std::size_t n) {
	for (std::size_t i = 0; i < n; ++i) {
		crc ^= p[i];
		for (int bit = 0; bit < 8; ++bit) { crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78u : 0); }
	}
	return crc;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	// First four bytes: the seed CRC. Next byte: the offset into the rest (alignment). The rest is the message.
	if (size < 5) { return 0; }
	std::uint32_t seed;
	std::memcpy(&seed, data, 4);
	std::size_t skew = data[4] % 16;
	data += 5; size -= 5;
	if (skew > size) { skew = size; }
	const unsigned char* bytes = data + skew;
	std::size_t n = size - skew;

	// The reference, over the whole message, from the same raw state as Crc32c(seed).
	std::uint32_t state = ~seed;
	std::uint32_t expected = ~bitwise(state, bytes, n);
	if (c32::detail::table_update(state, bytes, n) != ~expected) { std::abort(); }
	if (c32::detail::update(state, bytes, n) != ~expected) { std::abort(); }
	if (c32::Crc32c(seed).update(bytes, n).value() != expected) { std::abort(); }

	// Cut points taken from the message itself (pieces may be empty).
	c32::Crc32c crc(seed);
	std::size_t at = 0;
	for (std::size_t i = 0; i < n && at <= n; ++i) {
		std::size_t piece = bytes[i] % 97;
		if (piece > n - at) { piece = n - at; }
		crc.update(bytes + at, piece);
		at += piece;
	}
	crc.update(bytes + at, n - at);
	if (crc.value() != expected) { std::abort(); }

	// Combine: split in two at a cut from the message and join the halves.
	if (n) {
		std::size_t cut = bytes[0] % (n + 1);
		auto head = c32::Crc32c(seed).update(bytes, cut).value();
		auto tail = c32::crc32c(bytes + cut, n - cut);
		if (c32::crc32c_combine(head, tail, n - cut) != expected) { std::abort(); }
	}
	return 0;
}
