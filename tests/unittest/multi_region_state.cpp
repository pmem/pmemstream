// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* multi_region_state.cpp -- RapidCheck stateful test randomly adding and removing regions
	within a stream to check if region_allocator works properly. */

#include <vector>

#include <rapidcheck.h>

#include "common/util.h"
#include "rapidcheck_helpers.hpp"
#include "span.h"
#include "unittest.hpp"

/* increase the stream's size to fit in more regions */
#define STREAM_SIZE (TEST_DEFAULT_STREAM_SIZE * 128)
#define USABLE_SIZE (STREAM_SIZE - STREAM_METADATA_SIZE)

struct entry_data {
	uint64_t data;
};

struct regions_model {
	regions_model() : allocated_regions(), counter(0)
	{
		/* we require in this test to fit a single entry within each region */
		const size_t min_entry_size =
			ALIGN_UP(sizeof(span_entry) + sizeof(entry_data), TEST_DEFAULT_BLOCK_SIZE);
		region_size = *rc::gen::inRange<size_t>(min_entry_size, USABLE_SIZE);
		total_region_size = ALIGN_UP(region_size + sizeof(struct span_region), TEST_DEFAULT_BLOCK_SIZE);
	}
	/* each region holds an unique id; on pmem it's stored as an entry_data (within a region) */
	std::vector<size_t> allocated_regions;
	/* XXX: for now, region allocator requires all regions to be of the same size */
	size_t region_size;
	size_t total_region_size;
	/* provides an unique id for regions; it's incremented after an "add" command */
	size_t counter;
};

using regions_command = rc::state::Command<regions_model, pmemstream_test_base>;

struct rc_add_region : regions_command {

	void checkPreconditions(const regions_model &m) const override
	{
		RC_PRE((m.allocated_regions.size() + 1) * m.total_region_size <= USABLE_SIZE);
	}

	void apply(regions_model &m) const override
	{
		m.allocated_regions.push_back(m.counter);
		m.counter++;
	}

	void run(const regions_model &m, pmemstream_test_base &s) const override
	{
		auto [ret, reg] = s.sut.region_allocate(m.region_size);
		RC_ASSERT(ret == 0);
		/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
		RC_ASSERT(s.sut.region_size(reg) >= m.region_size);

		struct entry_data e = {.data = m.counter};
		ret = pmemstream_append(s.sut.c_ptr(), reg, NULL, &e, sizeof(e), NULL);
		RC_ASSERT(ret == 0);
	}
};

struct rc_failed_add_region : regions_command {
	void checkPreconditions(const regions_model &m) const override
	{
		RC_PRE((m.allocated_regions.size() + 1) * m.total_region_size > STREAM_SIZE);
	}

	void run(const regions_model &m, pmemstream_test_base &s) const override
	{
		auto [ret, reg] = s.sut.region_allocate(m.region_size);
		RC_ASSERT(ret == -1);
	}
};

struct rc_remove_region : regions_command {
	size_t idx_to_remove;

	explicit rc_remove_region(const regions_model &m)
	{
		RC_PRE(m.allocated_regions.size() > 0);
		idx_to_remove = *rc::gen::inRange<std::size_t>(0, m.allocated_regions.size());
	}

	void checkPreconditions(const regions_model &m) const override
	{
		/* it should be safe to remove (especially while shrinking the TC) */
		RC_PRE(m.allocated_regions.size() > idx_to_remove);
	}

	void apply(regions_model &m) const override
	{
		auto iter_to_remove = m.allocated_regions.begin() + static_cast<long>(idx_to_remove);
		m.allocated_regions.erase(iter_to_remove);
	}

	void run(const regions_model &m, pmemstream_test_base &s) const override
	{
		auto region = s.helpers.get_region(idx_to_remove);
		auto ret = s.sut.region_free(region);
		RC_ASSERT(ret == 0);
	}
};

struct rc_failed_remove_region : regions_command {
	void run(const regions_model &m, pmemstream_test_base &s) const override
	{
		struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};
		auto ret = s.sut.region_free(invalid_region);
		RC_ASSERT(ret == -1);
	}
};

struct rc_iterate_regions : regions_command {
	void run(const regions_model &m, pmemstream_test_base &s) const override
	{
		auto it = s.sut.region_iterator();
		size_t regions_count = 0;
		struct pmemstream_region region;
		while (pmemstream_region_iterator_next(it.get(), &region) != -1) {
			/* in each region we expect only 1 entry (storing the unique id) */
			auto entries = s.helpers.get_elements_in_region(region);
			RC_ASSERT(entries.size() == 1);

			auto data = reinterpret_cast<const struct entry_data *>(entries.back().data());

			RC_ASSERT(m.allocated_regions[regions_count] == data->data);
			regions_count++;
		}
		RC_ASSERT(regions_count == m.allocated_regions.size());
	}
};

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.stream_size = STREAM_SIZE;

	return run_test(test_config, [&] {
		return_check ret;
		ret += rc::check(
			"Adding and removing multiple regions should be iterable and accesible",
			[](pmemstream_empty &&stream) {
				struct regions_model model;

				rc::state::check(
					model, stream,
					rc::state::gen::execOneOfWithArgs<rc_add_region, rc_failed_add_region,
									  rc_failed_remove_region, rc_remove_region,
									  rc_iterate_regions>());
			});
	});
}
