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
	return run_test([]{
		rc::check([](const std::vector<uint64_t> &data){	
			size_t expected = 0;
			for (auto e : data) {
				for (size_t i = 0; i < sizeof(uint64_t); i++) {
					expected += e & (1ULL << i);
				}
			}

			const auto count = pmemstream_popcount(static_cast<const uint64_t*>(data.data()), data.size());
			UT_ASSERTeq(count, expected);
		});

		rc::check([](std::vector<uint64_t>&& data){			
			const auto middle = *rc::gen::inRange<std::size_t>(0, data.size() * sizeof(uint64_t));
			auto ptr = reinterpret_cast<uint8_t*>(data.data());

			std::memset(reinterpret_cast<void*>(ptr + middle), 1, data.size() - middle);

			const auto to_middle_count = pmemstream_popcount(reinterpret_cast<uint64_t*>(ptr), data.size() - middle);
			const auto all_count = pmemstream_popcount(reinterpret_cast<uint64_t*>(ptr), data.size());

			UT_ASSERTeq(to_middle_count + data.size() - middle, all_count);
		});

		rc::check([](std::vector<uint64_t> &&data){
			int ones_diff = 0;
			const auto first_count = static_cast<int>(pmemstream_popcount(data.data(), data.size()));

			const auto uniqueIndices = *rc::gen::unique<std::vector<size_t>>(rc::gen::inRange<size_t>(0, data.size()));
			for (auto index : uniqueIndices) {
				const auto byte = std::bitset<sizeof(uint64_t)>(data[index]);
				data[index] = *rc::gen::arbitrary<uint64_t>();
				const auto newByte = std::bitset<sizeof(uint64_t)>(data[index]);

				for (size_t i = 0; i < 64; i++) {
					ones_diff += newByte[i] - byte[i];
				}
			}

			const auto second_count = static_cast<int>(pmemstream_popcount(data.data(), data.size()));
			UT_ASSERTeq(first_count, second_count + ones_diff);
		});
	});
}
