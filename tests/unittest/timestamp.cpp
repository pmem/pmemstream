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

void multithreaded_asynchronous_append(pmemstream_test_base &stream, const std::vector<pmemstream_region> &regions,
				       const std::vector<std::string> &data, size_t concurrency_level)
{
	// add batch write
	// 0. Determine number of threads
	// 1. Divide data into smaller, per thread
	// 2.
	std::vector<std::vector<size_t>> data_partitioned(concurrency_level);
	for (size_t thread_id = 0; thread_id < concurrency_level; thread_id++) {
		int part_start = 0;
		int part_end = 0;
		do {
			part_end = *rc::gen::inRange<size_t>(static_cast<size_t>(part_start) + 1, data.size());
			// data_partitioned[thread_id].push_back(
			// 	std::vector<std::string>(data.begin() + part_start, data.begin() + part_end));
			part_start += part_end - part_start;
		} while (static_cast<size_t>(part_start) <= data.size());
	}
	// parallel_exec(concurrency_level, [&](size_t thread_id) {
	//	for (auto partition : data_partitioned[thread_id]) {
	//		auto future = stream.helpers.async_append(regions[thread_id], partition);
	//		while (future.poll() != FUTURE_STATE_COMPLETE)
	//			;
	//	}
	// });
}

void multithreaded_synchronous_append(pmemstream_test_base &stream, const std::vector<pmemstream_region> &regions,
				      const std::vector<std::string> &data)
{
	parallel_exec(regions.size(), [&](size_t thread_id) { stream.helpers.append(regions[thread_id], data); });
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
		ret += rc::check(
			"timestamp values should increase in each region after synchronous append",
			[&](pmemstream_with_multi_empty_regions &&stream, const std::vector<std::string> &data) {
				RC_PRE(data.size() > 0);
				auto regions = stream.helpers.get_regions();

				/* Multithreaded append to many regions with global ordering. */
				multithreaded_synchronous_append(stream, regions, data);

				/* Single region ordering validation. */
				for (auto &region : regions) {
					UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps({region}));
				}
			});

		ret += rc::check(
			"timestamp values should globally increase in multi-region environment after synchronous append",
			[&](pmemstream_with_multi_empty_regions &&stream, const std::vector<std::string> &data) {
				RC_PRE(data.size() > 0);
				auto regions = stream.helpers.get_regions();

				/* Multithreaded append to many regions with global ordering. */
				multithreaded_synchronous_append(stream, regions, data);

				/* Global ordering validation */
				UT_ASSERT(stream.helpers.validate_timestamps_no_gaps(regions));
			});

		ret += rc::check(
			"timestamp values should globally increase in multi-region environment after synchronous append to respawned region",
			[&](pmemstream_with_multi_empty_regions &&stream, const std::vector<std::string> &data,
			    const std::vector<std::string> &extra_data) {
				RC_PRE(data.size() > 0);
				RC_PRE(extra_data.size() > 0);
				auto regions = stream.helpers.get_regions();

				/* Multithreaded append to many regions with global ordering. */
				multithreaded_synchronous_append(stream, regions, data);

				size_t pos = *rc::gen::inRange<size_t>(0, regions.size());
				auto region_to_remove = regions[pos];
				auto region_size = stream.sut.region_size(region_to_remove);
				UT_ASSERTeq(stream.helpers.remove_region(region_to_remove.offset), 0);
				regions.erase(regions.begin() + static_cast<int>(pos));

				/* Global ordering validation. */
				if (regions.size() >= 1)
					UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));

				regions.push_back(stream.helpers.initialize_single_region(region_size, extra_data));
				UT_ASSERTeq(stream.helpers.get_entries_from_regions(regions).size(),
					    (regions.size() - 1) * data.size() + extra_data.size());
				UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));
			});

		// XXX: implement asynchronous cases
		ret += rc::check("timestamp values should increase in each region after asynchronous append",
				 [&](pmemstream_with_multi_empty_regions &&stream, const std::vector<std::string> &data,
				     const std::vector<std::string> &extra_data) {
					 RC_PRE(data.size() > 0);
					 auto regions = stream.helpers.get_regions();
					 size_t concurrency_level =
						 std::min(regions.size(), test_config.max_concurrency);
					 std::cout << "case start" << std::endl;
					 /* Multithreaded append to many regions with global ordering. */
					 multithreaded_asynchronous_append(stream, regions, data, concurrency_level);
					 std::cout << "case end" << std::endl;
					 /* Single region ordering validation. */
					 for (auto &region : regions) {
						 UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps({region}));
					 }
				 });

		ret += rc::check(
			"timestamp values should globally increase in multi-region environment after asynchronous append",
			[&](pmemstream_with_multi_empty_regions &&stream, const std::vector<std::string> &data) {
				RC_PRE(data.size() > 0);
				auto regions = stream.helpers.get_regions();
				size_t concurrency_level = std::min(regions.size(), test_config.max_concurrency);

				/* Multithreaded append to many regions with global ordering. */
				multithreaded_asynchronous_append(stream, regions, data, concurrency_level);

				/* Global ordering validation */
				UT_ASSERT(stream.helpers.validate_timestamps_no_gaps(regions));
			});

		ret += rc::check(
			"timestamp values should globally increase in multi-region environment after asynchronous append to respawned region",
			[&](pmemstream_with_multi_empty_regions &&stream, const std::vector<std::string> &data,
			    const std::vector<std::string> &extra_data) {
				RC_PRE(data.size() > 0);
				RC_PRE(extra_data.size() > 0);
				auto regions = stream.helpers.get_regions();
				size_t concurrency_level = std::min(regions.size(), test_config.max_concurrency);

				/* Multithreaded append to many regions with global ordering. */
				multithreaded_asynchronous_append(stream, regions, data, concurrency_level);

				size_t pos = *rc::gen::inRange<size_t>(0, regions.size());
				auto region_to_remove = regions[pos];
				auto region_size = stream.sut.region_size(region_to_remove);
				UT_ASSERTeq(stream.helpers.remove_region(region_to_remove.offset), 0);
				regions.erase(regions.begin() + static_cast<int>(pos));

				/* Global ordering validation. */
				if (regions.size() > 1)
					UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));

				{
					auto [ret, region] = stream.sut.region_allocate(region_size);
					UT_ASSERTeq(ret, 0);
					regions.push_back(region);

					auto future = stream.helpers.async_append(region, extra_data);
					while (future.poll() != FUTURE_STATE_COMPLETE)
						;
				}

				UT_ASSERTeq(stream.helpers.get_entries_from_regions(regions).size(),
					    (regions.size() - 1) * data.size() + extra_data.size());
				UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));
			});
	});
}
