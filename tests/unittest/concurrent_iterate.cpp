// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <vector>

#include "rapidcheck_helpers.hpp"
#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t max_concurrency = 8;

/* Stream big enough to fit test cases */
static constexpr size_t STREAM_SIZE =
	TEST_DEFAULT_REGION_MULTI_SIZE * rc::DEFAULT_ELEMENT_COUNT * rc::DEFAULT_ELEMENT_COUNT;

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.stream_size = STREAM_SIZE;

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check("verify if each concurrent iteration observes the same data",
				 [&](pmemstream_with_multi_non_empty_regions &&stream, bool reopen,
				     concurrency_type<1, max_concurrency> concurrency) {
					 if (reopen)
						 stream.reopen();

					 auto regions = stream.helpers.get_regions();

					 for (const auto &region : regions) {
						 syncthreads_barrier syncthreads(concurrency);

						 std::vector<std::vector<std::string>> threads_data(concurrency);
						 parallel_exec(concurrency, [&](size_t tid) {
							 syncthreads();
							 threads_data[tid] =
								 stream.helpers.get_elements_in_region(region);
						 });

						 UT_ASSERT(all_of(threads_data, predicates::equal(threads_data[0])));
					 }
				 });
	});
}
