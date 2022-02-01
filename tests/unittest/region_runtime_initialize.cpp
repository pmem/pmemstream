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

		ret += rc::check(
			"verify pmemstream_region_runtime_initialize return the same value for all threads", [&]() {
				const auto concurrency = *rc::gen::inRange<std::size_t>(0, max_concurrency);

				auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
				auto region =
					initialize_stream_single_region(stream.get(), TEST_DEFAULT_REGION_SIZE, {});

				std::vector<pmemstream_region_runtime *> threads_data(concurrency);
				parallel_exec(concurrency, [&](size_t tid) {
					UT_ASSERT(pmemstream_region_runtime_initialize(stream.get(), region,
										       &threads_data[tid]) == 0);

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

					 auto garbage_destination = [](pmemstream *stream,
								       pmemstream_entry last_entry) {
						 auto *cdata = reinterpret_cast<const char *>(
							 pmemstream_entry_data(stream, last_entry));
						 auto *data = const_cast<char *>(cdata);

						 /* garbage_destination is surely bigger than end offset of last_entry
						  * (including any padding). */
						 return data + pmemstream_entry_length(stream, last_entry) +
							 TEST_DEFAULT_BLOCK_SIZE;
					 };

					 {
						 auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE,
									       TEST_DEFAULT_STREAM_SIZE);
						 region = initialize_stream_single_region(
							 stream.get(), TEST_DEFAULT_REGION_SIZE, data);

						 last_entry = get_last_entry(stream.get(), region);
						 auto garbage_dst = garbage_destination(stream.get(), last_entry);

						 std::memcpy(garbage_dst, garbage.data(), garbage.size());
					 }

					 {
						 auto stream = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE,
									       TEST_DEFAULT_STREAM_SIZE, false);

						 pmemstream_region_runtime *region_runtime;
						 (void)pmemstream_region_runtime_initialize(stream.get(), region,
											    &region_runtime);

						 auto garbage_dst = garbage_destination(stream.get(), last_entry);
						 for (size_t i = 0; i < garbage.size(); i++) {
							 UT_ASSERT(garbage_dst[i] == 0);
						 }
					 }
				 });
	});
}
