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

	auto path = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		ret += rc::check(
			"verify if mixing reserve+publish with append works fine",
			[&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
			    const bool use_append, const bool is_runtime_initialized) {
				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
				auto region =
					initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE, data);
				verify(stream.get(), region, data, {});

				reserve_and_publish(stream.get(), region, is_runtime_initialized, extra_data);

				verify(stream.get(), region, data, extra_data);
			});
	});
}
