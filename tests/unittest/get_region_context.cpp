// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <rapidcheck.h>
#include <vector>

#include "common/util.h"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

static constexpr size_t stream_size = 1024 * 1024;
static constexpr size_t max_concurrency = 56;

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file" << std::endl;
		return -1;
	}

	auto file = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		ret += rc::check("verify pmemstream_get_region_context return the same value for all threads", [&]() {
			static constexpr size_t region_size = stream_size - 16 * 1024;
			static constexpr size_t block_size = 4096;
			const auto concurrency = *rc::gen::inRange<std::size_t>(0, max_concurrency);

			auto stream = make_pmemstream(file, block_size, stream_size);
			auto region = initialize_stream_single_region(stream.get(), region_size, {});

			std::vector<pmemstream_region_context *> threads_data(concurrency);
			parallel_exec(concurrency, [&](size_t tid) {
				RC_ASSERT(pmemstream_get_region_context(stream.get(), region, &threads_data[tid]) == 0);

				auto is_nullptr = threads_data[tid] == nullptr;
				RC_ASSERT(!is_nullptr);
			});

			RC_ASSERT(all_equal(threads_data));
		});
	});
}
