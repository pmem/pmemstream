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
		std::cout << "Usage: " << argv[0] << " file" << std::endl;
		return -1;
	}

	auto file = std::string(argv[1]);

	return run_test([&] {
		return_check ret;

		ret += rc::check("verify if each concurrent iteration observes the same data",
				 [&](const std::vector<std::string> &data) {
					 auto stream = make_pmemstream(file, BLOCK_SIZE, STREAM_SIZE);
					 auto region = initialize_stream_single_region(stream.get(), REGION_SIZE, data);

					 std::vector<std::vector<std::string>> threads_data(concurrency);
					 parallel_exec(concurrency, [&](size_t tid) {
						 threads_data[tid] = get_elements_in_region(stream.get(), region);
					 });

					 for (const auto &d : threads_data) {
						 RC_ASSERT(data == d);
					 }

					 RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
				 });
		ret += rc::check(
			"verify if each concurrent iteration observes the same data after stream reopen",
			[&](const std::vector<std::string> &data) {
				pmemstream_region region;
				{
					auto stream = make_pmemstream(file, BLOCK_SIZE, STREAM_SIZE);
					region = initialize_stream_single_region(stream.get(), REGION_SIZE, data);
					verify(stream.get(), region, data, {});
				}
				{
					auto stream = make_pmemstream(file, BLOCK_SIZE, STREAM_SIZE, false);
					std::vector<std::vector<std::string>> threads_data(concurrency);
					parallel_exec(concurrency, [&](size_t tid) {
						threads_data[tid] = get_elements_in_region(stream.get(), region);
					});

					for (const auto &d : threads_data) {
						RC_ASSERT(data == d);
					}

					RC_ASSERT(pmemstream_region_free(stream.get(), region) == 0);
				}
			});
	});
}
