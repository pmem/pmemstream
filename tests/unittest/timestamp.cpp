// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <rapidcheck.h>

// #include "../examples/examples_helpers.hpp"
#include "libpmemstream.h"
#include "libpmemstream_internal.h"
#include "rapidcheck_helpers.hpp"
#include "span.h"
#include "stream_helpers.hpp"
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

/* pmememstream entry iterator wrapper, which helps to manage entries from
 * different regions in global order. */
class entry_iterator {
 public:
	entry_iterator(pmemstream *stream, pmemstream_region &region) : stream(stream)
	{
		struct pmemstream_entry_iterator *new_entry_iterator;
		if (pmemstream_entry_iterator_new(&new_entry_iterator, stream, region) != 0) {
			throw std::runtime_error("Cannot create entry iterators");
		}
		it = std::shared_ptr<pmemstream_entry_iterator>(new_entry_iterator, [](pmemstream_entry_iterator *eit) {
			pmemstream_entry_iterator_delete(&eit);
		});
		pmemstream_entry_iterator_seek_first(it.get());
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0) {
			throw std::runtime_error("No entries to iterate");
		}
	}

	void operator++()
	{
		pmemstream_entry_iterator_next(it.get());
	}

	bool operator<(entry_iterator &other)
	{
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0)
			return false;

		if (pmemstream_entry_iterator_is_valid(other.it.get()) != 0)
			return true;

		return get_timestamp() < other.get_timestamp();
	}

	payload get_data()
	{
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0) {
			throw std::runtime_error("Invalid iterator");
		}
		return *reinterpret_cast<const payload *>(
			pmemstream_entry_data(stream, pmemstream_entry_iterator_get(it.get())));
	}

	uint64_t get_timestamp()
	{
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0) {
			throw std::runtime_error("Invalid iterator");
		}

		auto this_entry = pmemstream_entry_iterator_get(it.get());
		return pmemstream_entry_timestamp(stream, this_entry);
	}

	bool is_valid()
	{
		return pmemstream_entry_iterator_is_valid(it.get()) == 0;
	}

	pmemstream_entry_iterator *raw_iterator()
	{
		return it.get();
	}

 private:
	pmemstream *stream;
	std::shared_ptr<pmemstream_entry_iterator> it;
};

std::vector<entry_iterator> get_entry_iterators(pmemstream *stream, std::vector<pmemstream_region> regions)
{
	std::vector<entry_iterator> entry_iterators;
	for (auto &region : regions) {
		entry_iterators.emplace_back(entry_iterator(stream, region));
	}
	return entry_iterators;
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
				std::vector<entry_iterator> entry_iterators =
					get_entry_iterators(stream.sut.c_ptr(), regions);
				uint64_t prev_timestamp = PMEMSTREAM_INVALID_TIMESTAMP;
				size_t entry_counter = 0;
				for (auto e_iterator : entry_iterators) {
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
				entry_iterators = get_entry_iterators(stream.sut.c_ptr(), regions);
				uint64_t expected_timestamp = PMEMSTREAM_FIRST_TIMESTAMP;
				for (size_t i = 0; i < no_elements * no_regions; i++) {
					auto oldest_data =
						std::min_element(entry_iterators.begin(), entry_iterators.end());
					payload entry = oldest_data->get_data();
					UT_ASSERTeq(expected_timestamp++, oldest_data->get_timestamp());
					++(*oldest_data);
				}
			});
	});
}