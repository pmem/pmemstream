// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file" << std::endl;
		return -1;
	}

	auto file = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		/* 1. Allocate region and init it with data.
		 * 2. Verify that all data matches.
		 * 3. Append extra_data to the end.
		 * 4. Verify that all data matches.
		 */
		ret += rc::check("verify if iteration return proper elements after append",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data) {
					 auto stream = make_pmemstream(file, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
					 auto region = initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE, data);
					 verify(stream.get(), region, data, {});
					 append(stream.get(), region, NULL, extra_data);
					 verify(stream.get(), region, data, extra_data);
					 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
				 });

		ret += rc::check("verify if iteration return proper elements after pmemstream reopen",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
				     bool user_created_context) {
					 pmemstream_region region;
					 {
						 auto stream = make_pmemstream(file, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
						 region = initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE,
											  data);
						 verify(stream.get(), region, data, {});
					 }
					 {
						 auto stream = make_pmemstream(file, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, false);
						 verify(stream.get(), region, data, {});
						 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
					 }
				 });

		ret += rc::check("verify if iteration return proper elements after append after pmemstream reopen",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
				     bool user_created_context) {
					 pmemstream_region region;
					 {
						 auto stream = make_pmemstream(file, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
						 region = initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE,
											  data);
						 verify(stream.get(), region, data, {});
					 }
					 {
						 auto stream = make_pmemstream(file, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, false);
						 pmemstream_region_context *ctx = NULL;
						 if (user_created_context) {
							 pmemstream_get_region_context(stream.get(), region, &ctx);
						 }

						 append(stream.get(), region, ctx, extra_data);
						 verify(stream.get(), region, data, extra_data);
						 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
					 }
				 });
	});
}
