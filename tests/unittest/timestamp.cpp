// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "libpmemstream.h"
#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.hpp"

/**
 * timestamp - unit test for testing method pmemstream_entry_timestamp()
 */

void multithreaded_synchronous_append(pmemstream_test_base &stream, const std::vector<pmemstream_region> &regions,
				      const std::vector<std::vector<std::string>> &data)
{
	parallel_exec(data.size(), [&](size_t thread_id) {
		for (auto &chunk : data) {
			stream.helpers.append(regions[thread_id], chunk);
		}
	});
}

size_t get_concurrency_level(test_config_type &config, const std::vector<pmemstream_region> &regions)
{
	return std::min(regions.size(), config.max_concurrency);
}

std::tuple<std::vector<pmemstream_region>, size_t> generate_and_append_data(pmemstream_with_multi_empty_regions &stream,
									    test_config_type &test_config, bool async)
{
	auto regions = stream.helpers.get_regions();
	size_t concurrency_level = get_concurrency_level(test_config, regions);

	/* Multithreaded append to many regions with global ordering. */
	const auto data = *rc::gen::container<std::vector<std::vector<std::string>>>(
		concurrency_level, rc::gen::arbitrary<std::vector<std::string>>());

	if (async) {
		// XXX: multithreaded_asynchronous_append(stream, regions, data);
		UT_ASSERT(false);
	} else {
		multithreaded_synchronous_append(stream, regions, data);
	}

	size_t elements = 0;
	for (auto &chunk : data) {
		elements += chunk.size();
	}

	return std::tie(regions, elements);
}

size_t remove_random_region(pmemstream_with_multi_empty_regions &stream, std::vector<pmemstream_region> &regions,
			    size_t concurrency_level)
{
	size_t pos = *rc::gen::inRange<size_t>(0, concurrency_level);
	auto region_to_remove = regions[pos];
	auto region_size = stream.sut.region_size(region_to_remove);
	UT_ASSERTeq(stream.helpers.remove_region(region_to_remove.offset), 0);
	regions.erase(regions.begin() + static_cast<int>(pos));
	return region_size;
}

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
		test_config = get_test_config();
		ret += rc::check("timestamp values should increase in each region after synchronous append",
				 [&](pmemstream_with_multi_empty_regions &&stream) {
					 auto [regions, elements] =
						 generate_and_append_data(stream, test_config, false /* sync */);

					 /* Single region ordering validation. */
					 for (auto &region : regions) {
						 UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps({region}));
					 }
				 });

		ret += rc::check(
			"timestamp values should globally increase in multi-region environment after synchronous append",
			[&](pmemstream_with_multi_empty_regions &&stream) {
				auto [regions, elements] =
					generate_and_append_data(stream, test_config, false /* sync */);

				/* Global ordering validation */
				UT_ASSERT(stream.helpers.validate_timestamps_no_gaps(regions));
			});

		ret += rc::check(
			"timestamp values should globally increase in multi-region environment after synchronous append to respawned region",
			[&](pmemstream_with_multi_empty_regions &&stream, const std::vector<std::string> &extra_data) {
				RC_PRE(extra_data.size() > 0);

				auto [regions, elements] =
					generate_and_append_data(stream, test_config, false /* sync */);
				auto concurrency_level = get_concurrency_level(test_config, regions);

				auto region_size = remove_random_region(stream, regions, concurrency_level);

				/* Global ordering validation. */
				if (regions.size() >= 1)
					UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));

				UT_ASSERTeq(stream.helpers.get_entries_from_regions(regions).size(),
					    elements * (concurrency_level - 1));

				regions.push_back(stream.helpers.initialize_single_region(region_size, extra_data));

				UT_ASSERTeq(stream.helpers.get_entries_from_regions(regions).size(),
					    elements * (concurrency_level - 1) + extra_data.size());
				UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));
			});
	});
}
