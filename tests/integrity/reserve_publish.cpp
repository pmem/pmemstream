// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * reserve_publish.cpp -- pmemstream_reserve and pmemstream_publish integrity test.
 *			It checks if we reserve-publish approach properly writes data on pmem.
 */

#include <cstring>
#include <vector>

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		UT_FATAL("usage: %s file-path", argv[0]);
	}

	auto path = std::string(argv[1]);

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.stream_size = TEST_DEFAULT_STREAM_SIZE;
	test_config.block_size = TEST_DEFAULT_BLOCK_SIZE;

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check("verify if mixing reserve+publish with append works fine",
				 [&](pmemstream_with_single_empty_region &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data) {
					 auto region = stream.helpers.get_first_region();
					 stream.helpers.append(region, data);
					 stream.helpers.reserve_and_publish(region, extra_data);
					 stream.helpers.verify(region, data, extra_data);
				 });
	});
}
