// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <rapidcheck.h>

// #include "../examples/examples_helpers.hpp"
#include "libpmemstream.h"
#include "libpmemstream_internal.h"
#include "rapidcheck_helpers.hpp"
#include "span.h"
#include "stream_helpers.hpp"
#include "thread_helpers.hpp"
#include "unittest.h"

#include <iostream>
#include <string.h>

/**
 * timestamp - unit test for testing method pmemstream_entry_timestamp()
 */

struct entry_data {
	uint64_t data;
};

/* User data. */
struct payload {
	payload() : produced_by(invalid_value), index(invalid_value)
	{
	}
	payload(size_t produced_by, size_t index) : produced_by(produced_by), index(index)
	{
	}

	static constexpr size_t invalid_value = std::numeric_limits<size_t>::max();

	size_t produced_by;
	size_t index;
};

std::ostream &operator<<(std::ostream &os, const payload &data)
{
	os << " produced by thread " << data.produced_by << " with index " << data.index;
	return os;
}

template <typename T>
void multithreaded_synchronous_append(T &stream, std::vector<pmemstream_region> regions, size_t no_elements)
{
	parallel_exec(regions.size(), [&](size_t thread_id) {
		for (size_t i = 0; i < no_elements; i++) {
			payload entry(thread_id, i);
			auto ret = pmemstream_append(stream.sut.c_ptr(), regions[thread_id], NULL, &entry,
						     sizeof(entry), NULL);
			UT_ASSERTeq(ret, 0);
		}
	});
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
		// cases:
		// (prereq1 case1) "verify increasing timestamp values in each region after synchronous append"
		// (prereq2 case1) "verify increasing timestamp values in each region after asynchronous append"
		// (prereq1 case2) "verify globally increasing timestamp values in multi region environment after
		// synchronous append"
		// (prereq2 case2) "verify globally increasing timestamp values in multi region
		// environment after asynchronous append" case 3
		ret += rc::check("verify increasing timestamp values in each region after synchronous append",
				 [&](pmemstream_with_multi_empty_regions &&stream,
				     ranged<size_t, 1, TEST_DEFAULT_REGION_MULTI_MAX_COUNT> no_elements) {
					 auto regions = stream.helpers.get_regions();

					 /* Multithreaded append to many regions with global ordering */
					 multithreaded_synchronous_append(stream, regions, no_elements);

					 // Single region ordering validation
					 for (auto &region : regions) {
						 UT_ASSERTeq(stream.helpers.validate_timestamps({region}, true), true);
					 }
				 });

		ret += rc::check(
			"verify globally increasing timestamp values in multi region environment after synchronous append",
			[&](pmemstream_with_multi_empty_regions &&stream,
			    ranged<size_t, 1, TEST_DEFAULT_REGION_MULTI_MAX_COUNT> no_elements) {
				auto regions = stream.helpers.get_regions();

				/* Multithreaded append to many regions with global ordering */
				multithreaded_synchronous_append(stream, regions, no_elements);

				// Global ordering validation
				UT_ASSERTeq(stream.helpers.validate_timestamps(regions, false), true);
			});

		ret += rc::check(
			"verify globally increasing timestamp values in multi region environment after synchronous append to respawned region",
			[&](pmemstream_with_multi_empty_regions &&stream,
			    ranged<size_t, 1, TEST_DEFAULT_REGION_MULTI_MAX_COUNT> no_elements,
			    const std::vector<std::string> &data) {
				RC_PRE(data.size() > 0);
				auto regions = stream.helpers.get_regions();

				/* Multithreaded append to many regions with global ordering */
				multithreaded_synchronous_append(stream, regions, no_elements);

				size_t pos = *rc::gen::inRange<size_t>(0, regions.size());
				auto region_to_remove = regions[pos];
				auto region_size = stream.sut.region_size(region_to_remove);
				UT_ASSERTeq(stream.helpers.remove_region(region_to_remove.offset), 0);
				regions.erase(regions.begin() + static_cast<int>(pos));

				// Global ordering validation
				UT_ASSERTeq(stream.helpers.validate_timestamps(regions, true), true);

				regions[pos] = stream.helpers.initialize_single_region(region_size, data);

				UT_ASSERTeq(stream.helpers.validate_timestamps(regions, true), true);
			});

		ret += rc::check(
			"verify globally increasing timestamp values in multi region environment after synchronous append to respawned region",
			[&](pmemstream_with_multi_empty_regions &&stream,
			    ranged<size_t, 1, TEST_DEFAULT_REGION_MULTI_MAX_COUNT> no_elements) {
				// Fill asynchronously

				// Check
			});
	});
}
