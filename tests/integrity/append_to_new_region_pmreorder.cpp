// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "append_pmreorder.hpp"
#include "libpmemstream_internal.h"
#include "stream_helpers.hpp"
#include "unittest.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

constexpr size_t regions_count = 2;
constexpr size_t entries_in_region_count = 4;

void check_consistency(test_config_type test_config)
{
	auto copy_path = copy_file(test_config.filename);
	bool no_truncate = false;
	bool no_init = false;

	struct pmemstream_test_base stream(copy_path, test_config.block_size, test_config.stream_size, no_truncate,
					   no_init);

	for (auto &region : stream.helpers.allocate_regions(2, TEST_DEFAULT_REGION_SIZE)) {
		stream.sut.append(region, "new data");
	}
	stream.helpers.validate_timestamps_no_gaps(stream.helpers.get_regions());
}

static std::string usage_msg(std::string app_name)
{
	return "Usage: " + app_name + " <create|fill|check> file-path";
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		std::cout << usage_msg(argv[0]) << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	std::string mode = argv[1];
	test_config.filename = argv[2];
	/* requested region_size in this test is of "block_size", but it's actually double that, because of aligning */
	test_config.stream_size =
		STREAM_METADATA_SIZE + 2 * TEST_DEFAULT_BLOCK_SIZE * regions_count * entries_in_region_count * 10;

	return run_test(test_config, [&] {
		if (mode == "create") {
			bool truncate = true;
			bool do_init = true;
			struct pmemstream_test_base stream(test_config.filename, test_config.block_size,
							   test_config.stream_size, truncate, do_init);

		} else if (mode == "fill") {
			init_random();
			fill(test_config, regions_count, entries_in_region_count);
		} else if (mode == "check") {
			check_consistency(test_config);
		} else {
			UT_FATAL("Wrong mode given!\n %s \n", usage_msg(argv[0]).c_str());
		}
	});
}
