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
	auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_REGION_SIZE);
	while (ret != -1) {
		std::tie(ret, region) = stream.helpers.stream.region_allocate(TEST_DEFAULT_REGION_SIZE);
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
	test_config.filename = argv[1];
	test_config.stream_size = TEST_DEFAULT_STREAM_SIZE * 10;

	return run_test(test_config, [&] {
		return_check ret;
		size_t max_allocations = count_max_regions(test_config);
		RC_ASSERT(max_allocations != 0);

		ret += rc::check("Each allocated region may be freed", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);

			for (size_t i = 0; i < no_regions; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_REGION_SIZE);
				RC_ASSERT(ret == 0);
			}

			RC_ASSERT(no_regions == stream.helpers.count_regions());

			stream.helpers.remove_regions(no_regions);
			RC_ASSERT(0 == stream.helpers.count_regions());
		});

		ret += rc::check("Some of allocated regions can be freed", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);
			size_t to_delete = *rc::gen::inRange<std::size_t>(1, no_regions);

			for (size_t i = 0; i < no_regions; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_REGION_SIZE);
				RC_ASSERT(ret == 0);
			}

			RC_ASSERT(no_regions == stream.helpers.count_regions());

			stream.helpers.remove_regions(to_delete);
			RC_ASSERT(no_regions - to_delete == stream.helpers.count_regions());
		});

		ret += rc::check("Random region can be freed", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);
			size_t to_delete = *rc::gen::inRange<std::size_t>(0, no_regions);

			for (size_t i = 0; i < no_regions; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_REGION_SIZE);
				RC_ASSERT(ret == 0);
			}

			RC_ASSERT(no_regions == stream.helpers.count_regions());

			stream.helpers.remove_region_at(to_delete);
			RC_ASSERT(no_regions - 1 == stream.helpers.count_regions());
		});

		ret += rc::check("Allocate regions after free", [&](pmemstream_empty &&stream) {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);
			size_t to_delete = *rc::gen::inRange<std::size_t>(1, no_regions);

			for (size_t i = 0; i < no_regions; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_REGION_SIZE);
				RC_ASSERT(ret == 0);
			}

			RC_ASSERT(no_regions == stream.helpers.count_regions());

			size_t tbd = to_delete;
			size_t pos = no_regions;
			while (tbd-- != 0) {
				stream.helpers.remove_region_at(--pos);
			}

			RC_ASSERT(no_regions - to_delete == stream.helpers.count_regions());

			for (size_t i = 0; i < to_delete; i++) {
				auto [ret, region] = stream.helpers.stream.region_allocate(TEST_DEFAULT_REGION_SIZE);
				RC_ASSERT(ret == 0);
			}
			RC_ASSERT(no_regions == stream.helpers.count_regions());
		});
	});
}
