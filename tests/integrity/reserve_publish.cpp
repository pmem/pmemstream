// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * reserve_publish.cpp -- pmemstream_reserve and pmemstream_publish integrity test.
 *			It checks if we reserve-publish approach properly writes data on pmem.
 */

#include <cstring>
#include <vector>

#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		UT_FATAL("usage: %s file-path", argv[0]);
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check("verify if mixing reserve+publish with append works fine",
				 [](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
				    const bool use_append, const bool is_runtime_initialized) {
					 pmemstream_sut stream(get_test_config().filename, TEST_DEFAULT_BLOCK_SIZE,
							       TEST_DEFAULT_STREAM_SIZE);
					 auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE,
											       data);
					 stream.helpers.verify(region, data, {});

					 if (!is_runtime_initialized)
						 stream.region_runtime_initialize(region);

					 stream.helpers.reserve_and_publish(region, extra_data);

					 stream.helpers.verify(region, data, extra_data);
				 });
	});
}
