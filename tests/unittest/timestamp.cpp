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
		ret += rc::check("timestamp values should increase in each region after synchronous append",
				 [&](pmemstream_with_multi_empty_regions<true, true> &&stream,
				     const std::vector<std::string> &data) {
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
			[&](pmemstream_with_multi_empty_regions<true, true> &&stream,
			    const std::vector<std::string> &data) {
				RC_PRE(data.size() > 0);
				auto regions = stream.helpers.get_regions();

				/* Multithreaded append to many regions with global ordering. */
				multithreaded_synchronous_append(stream, regions, data);

				/* Global ordering validation */
				UT_ASSERT(stream.helpers.validate_timestamps_no_gaps(regions));
			});

		ret += rc::check(
			"timestamp values should globally increase in multi-region environment after synchronous append to respawned region",
			[&](pmemstream_with_multi_empty_regions<true, true> &&stream,
			    const std::vector<std::string> &data, const std::vector<std::string> &extra_data) {
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
				UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));

				regions[pos] = stream.helpers.initialize_single_region(region_size, extra_data);

				UT_ASSERT(stream.helpers.validate_timestamps_possible_gaps(regions));
			});

		// XXX: implement asynchronous cases
	});
}
