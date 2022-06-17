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
		ret += rc::check(
			"verify increasing timestamp values in each region after synchronous append",
			[&](pmemstream_test_base &&stream) {
				size_t no_elements = *rc::gen::inRange<size_t>(1, 15);
				size_t no_regions = *rc::gen::inRange<size_t>(1, TEST_DEFAULT_REGION_MULTI_MAX_COUNT);

				std::vector<pmemstream_region> regions =
					stream.helpers.allocate_regions(no_regions, TEST_DEFAULT_REGION_MULTI_SIZE);

				/* Asynchronous append to many regions with global ordering */
				parallel_exec(no_regions, [&](size_t thread_id) {
					for (size_t i = 0; i < no_elements; i++) {
						payload entry(thread_id, i);
						pmemstream_append(stream.sut.c_ptr(), regions[thread_id], NULL, &entry,
								  sizeof(entry), NULL);
					}
				});

				// In region monotonicity check
				auto entry_iterators =
					entry_iterator<payload>::get_entry_iterators(stream.sut.c_ptr(), regions);
				uint64_t prev_timestamp = PMEMSTREAM_INVALID_TIMESTAMP;
				size_t entry_counter = 0;
				for (auto &e_iterator : entry_iterators) {
					while (e_iterator.is_valid()) {
						uint64_t curr_timestamp = e_iterator.get_timestamp();
						UT_ASSERT(prev_timestamp < curr_timestamp);
						prev_timestamp = curr_timestamp;
						++e_iterator;
						++entry_counter;
					}
					prev_timestamp = PMEMSTREAM_INVALID_TIMESTAMP;
				}
				UT_ASSERTeq(entry_counter, no_elements * no_regions);

				// Global ordering validation
				UT_ASSERTeq(stream.helpers.validate_timestamps(false), true);
			});
	});
}
