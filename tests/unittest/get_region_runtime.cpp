// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "common/util.h"
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

	auto path = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		ret += rc::check("verify pmemstream_get_region_runtime return the same value for all threads", [&]() {
			const auto concurrency = *rc::gen::inRange<std::size_t>(0, max_concurrency);

			auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
			auto region = initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE, {});

			std::vector<pmemstream_region_runtime *> threads_data(concurrency);
			parallel_exec(concurrency, [&](size_t tid) {
				UT_ASSERT(pmemstream_get_region_runtime(stream.get(), region, &threads_data[tid]) == 0);

				auto is_nullptr = threads_data[tid] == nullptr;
				UT_ASSERT(!is_nullptr);
			});

			UT_ASSERT(all_equal(threads_data));
		});
	});
}
