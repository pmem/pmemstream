// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "common/util.h"
#include "libpmemstream_internal.h"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t max_concurrency = 56;

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

		ret += rc::check(
			"verify pmemstream_region_runtime_initialize return the same value for all threads",
			[&](ranged<size_t, 1, max_concurrency> concurrency) {
				pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
							    get_test_config().stream_size);
				auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});

				std::vector<pmemstream_region_runtime *> threads_data(concurrency);
				parallel_exec(concurrency, [&](size_t tid) {
					auto [ret, region_runtime] = stream.sut.region_runtime_initialize(region);
					UT_ASSERTeq(ret, 0);
					threads_data[tid] = region_runtime;

					auto is_nullptr = threads_data[tid] == nullptr;
					UT_ASSERT(!is_nullptr);
				});

				UT_ASSERT(all_equal(threads_data));
			});

		ret += rc::check(
			"verify that pmemstream is ready to be written to after region_runtime_initialize_for_write_locked",
			[&](pmemstream_with_single_init_region &&stream,
			    ranged<size_t, 1, max_concurrency> concurrency) {
				stream.reopen();
				auto region = stream.helpers.get_first_region();

				auto *c_stream = stream.sut.c_ptr();
				struct pmemstream_region_runtime *region_runtime;
				UT_ASSERTeq(region_runtimes_map_get_or_create(c_stream->region_runtimes_map, region,
									      &region_runtime),
					    0);

				parallel_exec(concurrency, [&](size_t tid) {
					UT_ASSERTeq(region_runtime_iterate_and_initialize_for_write_locked(
							    c_stream, region, region_runtime),
						    0);
				});
			});
	});
}
