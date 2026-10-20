// A tour of crc32c: one shot, incremental, constexpr, combine, and which path runs.
#include <crc32c/crc32c.hh>

#include <cstdio>
#include <string>
#include <string_view>

namespace c32 = kronuz::crc32c;

int main() {
	std::string_view header = "header:", body = "the payload bytes";

	// One shot, over a view or over a pointer and a size.
	auto whole = c32::crc32c(std::string(header) + std::string(body));
	std::printf("one shot        %08x\n", whole);

	// Incremental: feed the pieces in any number of calls, empty ones included.
	c32::Crc32c crc;
	crc.update(header).update(body.data(), body.size());
	std::printf("incremental     %08x %s\n", crc.value(), crc.value() == whole ? "(same)" : "(DIFFERENT)");

	// Constexpr: the standard check value, computed by the compiler.
	static_assert(c32::crc32c("123456789") == 0xe3069283u);
	std::printf("check value     %08x\n", c32::crc32c(std::string_view("123456789")));

	// Combine: the CRC of A followed by B from the two CRCs and B's length, without rereading.
	auto joined = c32::crc32c_combine(c32::crc32c(header), c32::crc32c(body), body.size());
	std::printf("combine         %08x %s\n", joined, joined == whole ? "(same)" : "(DIFFERENT)");

	// Resume from a stored CRC: seed a Crc32c with it and keep appending.
	auto resumed = c32::Crc32c(c32::crc32c(header)).update(body).value();
	std::printf("resumed         %08x %s\n", resumed, resumed == whole ? "(same)" : "(DIFFERENT)");

	std::printf("hardware path   %s\n", c32::detail::hardware_available() ? "yes" : "no (slice-by-8)");
	return joined == whole && resumed == whole && crc.value() == whole ? 0 : 1;
}
