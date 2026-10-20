// Replays seeded and generated inputs through the fuzz target without libFuzzer, so the same checks run as a test.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main() {
	std::mt19937_64 rng(20261020);
	std::vector<std::uint8_t> input;
	std::size_t runs = 0;
	for (std::size_t size : {0, 1, 4, 5, 6, 20, 63, 64, 65, 1029, 3077, 3200, 6200, 20000}) {
		for (int round = 0; round < 40; ++round) {
			input.resize(size);
			for (auto& b : input) { b = static_cast<std::uint8_t>(rng()); }
			LLVMFuzzerTestOneInput(input.data(), input.size());
			++runs;
		}
	}
	std::printf("crc32c fuzz replay: %zu inputs, ok\n", runs);
}
