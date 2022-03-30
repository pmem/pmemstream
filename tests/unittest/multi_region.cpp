// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

size_t count_max_regions(test_config_type &test_config)
{
	pmemstream_test_base stream(test_config.filename, test_config.block_size, test_config.stream_size);
	size_t max_allocations = 0;
	std::tuple<int, pmemstream_region> ret = stream.helpers.stream.region_allocate(TEST_DEFAULT_BLOCK_SIZE);
	while (std::get<0>(ret) != -1) {
		ret = stream.helpers.stream.region_allocate(TEST_DEFAULT_BLOCK_SIZE);
		++max_allocations;
	};
	return max_allocations;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);

	return run_test(test_config, [&] {
		return_check ret;
		size_t max_allocations = count_max_regions(test_config);
		UT_ASSERTne(max_allocations, 0);

		ret += rc::check("Multiple allocate", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);

			for (size_t i = 0; i < no_regions; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_BLOCK_SIZE);
				UT_ASSERTeq(ret, 0);
			}

			RC_ASSERT(no_regions == static_cast<size_t>(stream.helpers.count_regions()));
		});

		ret += rc::check("Multiple allocate - multiple free", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);
			size_t to_delete = *rc::gen::inRange<std::size_t>(1, no_regions);

			for (size_t i = 0; i < no_regions; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_BLOCK_SIZE);
				UT_ASSERTeq(ret, 0);
			}

			RC_ASSERT(no_regions == static_cast<size_t>(stream.helpers.count_regions()));

			stream.helpers.remove_regions(to_delete);
			RC_ASSERT(no_regions - to_delete == static_cast<size_t>(stream.helpers.count_regions()));
		});
	});
}
