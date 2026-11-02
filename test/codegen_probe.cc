// Compiled to assembly by test/check_codegen.cmake: a call of crc32c() must not go through an out-of-line
// detail::update(), which would put a second call and a frame of saved registers in front of every short buffer.
#include <crc32c/crc32c.hh>

extern "C" std::uint32_t crc32c_probe(const void* data, std::size_t size) {
	return kronuz::crc32c::crc32c(data, size);
}
