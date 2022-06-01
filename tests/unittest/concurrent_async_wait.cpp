// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <cstdint>
#include <vector>

#include "common/util.h"
#include "env_setter.hpp"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

#include "libpmemstream_internal.h"

static constexpr size_t min_write_concurrency = 1;
static constexpr size_t max_write_concurrency = 12;
static constexpr size_t max_size = 1024; /* Max number of elements in stream and max size of single entry. */
static constexpr size_t region_size =
	ALIGN_UP(max_write_concurrency * max_size * max_size * 10, 4096ULL); /* 10x-margin */
static constexpr size_t stream_size = (region_size + REGION_METADATA_SIZE) + STREAM_METADATA_SIZE;

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.stream_size = stream_size;
	test_config.filename = std::string(argv[1]);

	return run_test(test_config, [&] {
		return_check ret;

		std::string rapidcheck_config = "noshrink=1 max_size=" + std::to_string(max_size);
		env_setter setter("RC_PARAMS", rapidcheck_config, false);

		ret += rc::check(
			"verify if calling async_wait_persisted from multiple threads does not lead to any deadlocks",
			[&](pmemstream_empty &&stream, const std::vector<std::string> &data,
			    const std::vector<std::string> &extra_data, bool reopen,
			    ranged<size_t, min_write_concurrency, max_write_concurrency> concurrency) {
				auto region = stream.helpers.initialize_single_region(region_size, data);

				if (reopen)
					stream.reopen();

				using future_type = decltype(stream.helpers.async_append(region, extra_data));
				std::vector<future_type> futures;
				for (size_t i = 0; i < concurrency; i++) {
					futures.emplace_back(stream.helpers.async_append(region, extra_data));
				}

				parallel_exec(concurrency, [&](size_t tid) {
					while (futures[tid].poll() != FUTURE_STATE_COMPLETE)
						;
				});

				std::vector<std::string> all_extra_data;
				all_extra_data.reserve(concurrency * extra_data.size());
				for (size_t i = 0; i < concurrency; i++)
					all_extra_data.insert(all_extra_data.end(), extra_data.begin(),
							      extra_data.end());
				stream.helpers.verify(region, data, all_extra_data);
			});

		// XXX: extend above test with verification of persistent data inside parallel_exec

		// XXX: add test with concurrent appends (to multiple regions)
	});
}
