// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "common/util.h"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

#include "libpmemstream_internal.h"

static constexpr size_t max_write_concurrency = 8;
static constexpr size_t max_size = 100; /* Max number of elements in stream and max size of single entry. */
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
	test_config.rc_params["noshrink"] = "1";
	test_config.rc_params["max_size"] = std::to_string(max_size);

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check(
			"verify if calling async_wait_persisted from multiple threads does not lead to any deadlocks",
			[&](pmemstream_empty &&stream, const std::vector<std::string> &data,
			    std::vector<std::vector<std::string>> &&extra_data, bool reopen,
			    concurrency_type<1, max_write_concurrency> concurrency) {
				RC_PRE(extra_data.size() >= concurrency);

				auto extra_data_size = std::accumulate(
					extra_data.begin(), extra_data.end(), 0U,
					[](const auto &lhs, const auto &rhs) { return lhs + rhs.size(); });
				RC_TAG(extra_data_size);

				auto region = stream.helpers.initialize_single_region(region_size, data);

				if (reopen)
					stream.reopen();

				using future_type = decltype(stream.helpers.async_append(region, extra_data[0]));
				std::vector<std::vector<future_type>> futures(concurrency);
				for (size_t i = 0; i < extra_data.size(); i++) {
					futures[i % concurrency].emplace_back(
						stream.helpers.async_append(region, extra_data[i]));
				}

				for (auto &future_sequence : futures) {
					std::mt19937_64 g(*rc::gen::arbitrary<size_t>());
					std::shuffle(future_sequence.begin(), future_sequence.end(), g);
				}

				parallel_exec(concurrency, [&](size_t tid) {
					for (auto &future : futures[tid]) {
						while (future.poll() != FUTURE_STATE_COMPLETE)
							;
					}
				});

				std::vector<std::string> all_extra_data;
				all_extra_data.reserve(extra_data_size);
				for (auto &entry_sequence : extra_data) {
					all_extra_data.insert(all_extra_data.end(), entry_sequence.begin(),
							      entry_sequence.end());
				}
				stream.helpers.verify(region, data, all_extra_data);
			});

		// XXX: extend above test with verification of persistent data inside parallel_exec

		// XXX: add test with concurrent appends (to multiple regions)
	});
}
