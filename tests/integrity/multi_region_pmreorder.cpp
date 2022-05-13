// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "libpmemstream_internal.h"
#include "random_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

#include <filesystem>
#include <iostream>

#define TEST_COMMANDS_COUNT (50UL)

void fill(test_config_type test_config, size_t cmd_count = TEST_COMMANDS_COUNT)
{
	bool no_truncate = false;
	auto stream =
		make_pmemstream(test_config.filename, test_config.block_size, test_config.stream_size, no_truncate);

	const size_t region_size = TEST_DEFAULT_BLOCK_SIZE;
	std::vector<struct pmemstream_region> regions;

	for (size_t i = 0; i < cmd_count; i++) {
		auto command = rnd_generator() % 2; /* allocate & free */

		if (command == 0) {
			struct pmemstream_region region;
			int ret = pmemstream_region_allocate(stream.get(), region_size, &region);
			UT_ASSERTeq(ret, 0);

			regions.push_back(region);
		} else if (command == 1) {
			/* free (only if preceeded by any, non-freed allocation) */
			if (regions.size() == 0) {
				continue;
			}

			size_t region_pos = rnd_generator() % regions.size();
			auto region = regions[region_pos];

			int ret = pmemstream_region_free(stream.get(), region);
			UT_ASSERTeq(ret, 0);
			regions.erase(regions.begin() + static_cast<long>(region_pos));
		}
	}
}

void check_consistency(test_config_type test_config)
{
	auto copy_path = copy_file(test_config.filename);
	bool no_truncate = false;
	bool no_init = false;

	struct pmemstream_test_base s(copy_path, test_config.block_size, test_config.stream_size, no_truncate, no_init);

	auto riter = s.sut.region_iterator();
	pmemstream_region_iterator_seek_first(riter.get());
	int region_counter = 0;
	int ret = 0;
	while (pmemstream_region_iterator_is_valid(riter.get()) == 0) {
		++region_counter;
		pmemstream_region_iterator_next(riter.get());
	}

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(s.sut.c_ptr(), TEST_DEFAULT_BLOCK_SIZE, &region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(static_cast<size_t>(region_counter + 1), s.helpers.count_regions());

	ret = pmemstream_region_free(s.sut.c_ptr(), region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(static_cast<size_t>(region_counter), s.helpers.count_regions());
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << "<create|fill|check> file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	std::string mode = argv[1];
	test_config.filename = argv[2];
	/* requested region_size in this test is of "block_size", but it's actually double that, because of aligning */
	test_config.stream_size = STREAM_METADATA_SIZE + 2 * TEST_DEFAULT_BLOCK_SIZE * TEST_COMMANDS_COUNT;

	return run_test(test_config, [&] {
		if (mode == "create") {
			bool truncate = true;
			bool do_init = true;
			struct pmemstream_test_base stream(test_config.filename, test_config.block_size,
							   test_config.stream_size, truncate, do_init);

		} else if (mode == "fill") {
			init_random();
			fill(test_config);

		} else if (mode == "check") {
			check_consistency(test_config);

		} else {
			UT_FATAL("Wrong mode given!\nUsage: %s <create|fill|check> file-path", argv[0]);
		}
	});
}
