// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <algorithm>
#include <bitset>
#include <cstring>
#include <numeric>
#include <rapidcheck.h>
#include <vector>

#include "common/util.h"
#include "unittest.hpp"

int main()
{
	return run_test([] {
		return_check ret;

		ret += rc::check(
			"verify popcount against custom implementation", [](const std::vector<uint64_t> &data) {
				size_t expected = 0;
				for (auto e : data) {
					while (e) {
						expected += e & 1;
						e >>= 1;
					}
				}

				const auto size = data.size() * sizeof(uint64_t);
				const auto count =
					util_popcount_memory(reinterpret_cast<const uint8_t *>(data.data()), size);
				RC_ASSERT(count == expected);
			});

		ret += rc::check("verify if popcount handles sizes which are not multiple of 8",
				 [](std::vector<uint64_t> &&data) {
					 const auto size = data.size() * sizeof(uint64_t);
					 const auto middle =
						 *rc::gen::inRange<std::size_t>(0, data.size() * sizeof(uint64_t));
					 auto ptr = reinterpret_cast<uint8_t *>(data.data());

					 std::memset(reinterpret_cast<void *>(ptr + middle), 1, size - middle);

					 const auto to_middle_count = util_popcount_memory(ptr, middle);
					 const auto all_count = util_popcount_memory(ptr, size);

					 RC_ASSERT(to_middle_count + (size - middle) == all_count);
				 });

		ret += rc::check(
			"verify if popcount works after flipping random bytes", [](std::vector<uint64_t> &&data) {
				const auto size = data.size() * sizeof(uint64_t);
				const auto ptr = reinterpret_cast<uint8_t *>(data.data());

				/* Calculate popcount on the initial array. */
				const auto first_count = static_cast<int>(util_popcount_memory(ptr, size));

				int ones_diff = 0;

				/* Generate unique list of indices of data - those will be elements which we will
				 * randomly flip. */
				const auto uniqueIndices =
					*rc::gen::unique<std::vector<size_t>>(rc::gen::inRange<size_t>(0, data.size()));
				for (auto index : uniqueIndices) {
					const auto byte = std::bitset<64>(data[index]);

					/* Flip random bits. */
					data[index] = *rc::gen::arbitrary<uint64_t>();

					const auto newByte = std::bitset<64>(data[index]);

					/* Calculate how many more ones we have. */
					for (size_t i = 0; i < 64; i++) {
						ones_diff += newByte[i] - byte[i];
					}
				}

				/* Calculate popcount for the second time and make sure the result differs
				 * from the original one by exactly ones_diff. */
				const auto second_count = static_cast<int>(util_popcount_memory(ptr, size));
				RC_ASSERT(first_count + ones_diff == second_count);
			});
	});
}
