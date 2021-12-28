// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <algorithm>
#include <bitset>
#include <cstring>
#include <numeric>
#include <vector>

#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t stream_size = 1024 * 1024;

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
					 static constexpr size_t region_size = stream_size - 16 * 1024;
					 static constexpr size_t block_size = 4096;

					 auto stream = make_pmemstream(file, block_size, stream_size);
					 auto region = initialize_stream_single_region(stream.get(), region_size, data);
					 verify(stream.get(), region, data, {});
					 append(stream.get(), region, NULL, extra_data);
					 verify(stream.get(), region, data, extra_data);
					 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
				 });

		ret += rc::check("verify if iteration return proper elements after pmemstream reopen",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
				     bool user_created_context) {
					 static constexpr size_t region_size = stream_size - 16 * 1024;
					 static constexpr size_t block_size = 4096;
					 pmemstream_region region;
					 {
						 auto stream = make_pmemstream(file, block_size, stream_size);
						 region = initialize_stream_single_region(stream.get(), region_size,
											  data);
						 verify(stream.get(), region, data, {});
					 }
					 {
						 auto stream = make_pmemstream(file, block_size, stream_size, false);
						 verify(stream.get(), region, data, {});
						 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
					 }
				 });

		ret += rc::check("verify if iteration return proper elements after append after pmemstream reopen",
				 [&](const std::vector<std::string> &data, const std::vector<std::string> &extra_data,
				     bool user_created_context) {
					 static constexpr size_t region_size = stream_size - 16 * 1024;
					 static constexpr size_t block_size = 4096;
					 pmemstream_region region;
					 {
						 auto stream = make_pmemstream(file, block_size, stream_size);
						 region = initialize_stream_single_region(stream.get(), region_size,
											  data);
						 verify(stream.get(), region, data, {});
					 }
					 {
						 auto stream = make_pmemstream(file, block_size, stream_size, false);
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
