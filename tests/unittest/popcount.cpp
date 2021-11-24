#include <rapidcheck.h>
#include <vector>
#include <cstring>
#include <bitset>
#include <algorithm>
#include <numeric>

#include "unittest.hpp"

#include "common/util.h"

int main()
{
	return run_rc_test({
		rc::check("verify popcount agains custom implementation", [](const std::vector<uint64_t> &data){
			size_t expected = 0;
			for (auto e : data) {
				while (e) {
					expected += e & 1;
					e >>= 1;
				}
			}

			const auto size = data.size() * sizeof(uint64_t);
			const auto count = util_popcount_memory(reinterpret_cast<const uint8_t*>(data.data()), size);
			RC_ASSERT(count == expected);
		}),
		rc::check("verify if popcount handles sizes which is not multiple of 8", [](std::vector<uint64_t>&& data){
			const auto size = data.size() * sizeof(uint64_t);
			const auto middle = *rc::gen::inRange<std::size_t>(0, data.size() * sizeof(uint64_t));
			auto ptr = reinterpret_cast<uint8_t*>(data.data());

			std::memset(reinterpret_cast<void*>(ptr + middle), 1, size - middle);

			const auto to_middle_count = util_popcount_memory(ptr, middle);
			const auto all_count = util_popcount_memory(ptr, size);

			RC_ASSERT(to_middle_count + (size - middle) == all_count);
		}),
		rc::check("verify if popcount works after flipping random bytes", [](std::vector<uint64_t> &&data){
			const auto size = data.size() * sizeof(uint64_t);
			const auto ptr = reinterpret_cast<uint8_t*>(data.data());
			const auto first_count = static_cast<int>(util_popcount_memory(ptr, size));

			int ones_diff = 0;

			const auto uniqueIndices = *rc::gen::unique<std::vector<size_t>>(rc::gen::inRange<size_t>(0, data.size()));
			for (auto index : uniqueIndices) {
				const auto byte = std::bitset<64>(data[index]);
				data[index] = *rc::gen::arbitrary<uint64_t>();
				const auto newByte = std::bitset<64>(data[index]);

				for (size_t i = 0; i < 64; i++) {
					ones_diff += newByte[i] - byte[i];
				}
			}

			const auto second_count = static_cast<int>(util_popcount_memory(ptr, size));
			RC_ASSERT(first_count + ones_diff == second_count);
		})
	});
}
