// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <vector>

#include <rapidcheck.h>

#include "common/util.h"
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

	auto path = std::string(argv[1]);

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.stream_size = TEST_DEFAULT_STREAM_SIZE;
	test_config.block_size = TEST_DEFAULT_BLOCK_SIZE;

	return run_test(test_config, [&] {
		return_check ret;

		ret += rc::check(
			"verify pmemstream_region_runtime_initialize return the same value for all threads", [&]() {
				const auto concurrency = *rc::gen::inRange<std::size_t>(0, max_concurrency);

				pmemstream_test_base stream(path, get_test_config().block_size,
							    get_test_config().stream_size);
				auto region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});

				std::vector<pmemstream_region_runtime *> threads_data(concurrency);
				parallel_exec(concurrency, [&](size_t tid) {
					auto [ret, rr] = stream.sut.region_runtime_initialize(region);
					UT_ASSERTeq(ret, 0);
					threads_data[tid] = rr;

					auto is_nullptr = threads_data[tid] == nullptr;
					UT_ASSERT(!is_nullptr);
				});

				UT_ASSERT(all_equal(threads_data));
			});

		ret += rc::check("verify that pmemstream_region_runtime_initialize clears region after last entry",
				 [&](const std::vector<std::string> &data, const std::string &garbage) {
					 RC_PRE(data.size() > 0);

					 pmemstream_region region;
					 pmemstream_entry last_entry;

					 pmemstream_test_base stream(path, get_test_config().block_size,
								     get_test_config().stream_size);

					 auto garbage_destination = [&](pmemstream_entry last_entry) {
						 /* garbage_destination is surely bigger than end offset of last_entry
						  * (including any padding). */
						 auto last_entry_data = stream.sut.get_entry(last_entry);
						 auto *data = const_cast<char *>(last_entry_data.data());
						 return data + last_entry_data.size() + get_test_config().block_size;
					 };

					 region = stream.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE,
											  data);

					 last_entry = stream.helpers.get_last_entry(region);
					 auto garbage_dst = garbage_destination(last_entry);

					 std::memcpy(garbage_dst, garbage.data(), garbage.size());

					 stream.reopen();
					 stream.sut.region_runtime_initialize(region);

					 garbage_dst = garbage_destination(last_entry);
					 for (size_t i = 0; i < garbage.size(); i++) {
						 UT_ASSERTeq(garbage_dst[i], 0);
					 }
				 });
	});
}
