// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t concurrency = 4;

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.stream_size = TEST_DEFAULT_STREAM_SIZE;
	test_config.block_size = TEST_DEFAULT_BLOCK_SIZE;

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check(
			"verify if each concurrent iteration observes the same data",
			[&](const std::vector<std::string> &data, bool reopen) {
				pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
							    get_test_config().stream_size);
				auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, data);

				if (reopen)
					stream.reopen();

				std::vector<std::vector<std::string>> threads_data(concurrency);
				parallel_exec(concurrency, [&](size_t tid) {
					threads_data[tid] = stream.helpers.get_elements_in_region(region);
				});

				UT_ASSERT(all_of(threads_data, predicates::equal(data)));
				UT_ASSERTeq(stream.sut.region_free(region), 0);
			});
	});
}
