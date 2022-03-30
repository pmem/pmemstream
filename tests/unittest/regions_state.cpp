// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* regions_state.cpp -- RapidCheck stateful test randomly adding and removing regions
	within a stream to check if region_allocator works properly. */

#include <rapidcheck.h>

#include "common/util.h"
#include "rapidcheck_helpers.hpp"
#include "span.h"
#include "unittest.hpp"

#define STREAM_SIZE (TEST_DEFAULT_STREAM_SIZE * 128)
#define USABLE_SIZE (STREAM_SIZE - STREAM_METADATA_SIZE)

struct regions_model {
	regions_model() : allocated_regions(0)
	{
		/* XXX: for now, region allocator requires all regions to be of the same size */
		region_size = *rc::gen::inRange<size_t>(1, USABLE_SIZE);
		total_region_size = ALIGN_UP(region_size + sizeof(struct span_region), TEST_DEFAULT_BLOCK_SIZE);
	}
	size_t allocated_regions;
	size_t region_size;
	size_t total_region_size;
};

struct regions_test {
	regions_test(struct pmemstream_test_base &s) : sut(s)
	{
	}

	struct pmemstream_test_base &sut;
};

using regions_command = rc::state::Command<regions_model, regions_test>;

struct rc_add_region : regions_command {
	void checkPreconditions(const regions_model &m) const override
	{
		RC_PRE((m.allocated_regions + 1) * m.total_region_size <= USABLE_SIZE);
	}

	void apply(regions_model &m) const override
	{
		m.allocated_regions++;
	}

	void run(const regions_model &m, regions_test &s) const override
	{
		auto [ret, reg] = s.sut.sut.region_allocate(m.region_size);
		RC_ASSERT(ret == 0);
		/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
		RC_ASSERT(s.sut.sut.region_size(reg) >= m.region_size);
	}
};

struct rc_failed_add_region : regions_command {
	void checkPreconditions(const regions_model &m) const override
	{
		RC_PRE((m.allocated_regions + 1) * m.total_region_size > STREAM_SIZE);
	}

	void run(const regions_model &m, regions_test &s) const override
	{
		auto [ret, reg] = s.sut.sut.region_allocate(m.region_size);
		RC_ASSERT(ret == -1);
	}
};

struct rc_remove_region : regions_command {
	size_t idx_to_remove;

	explicit rc_remove_region(const regions_model &m)
	{
		RC_PRE(m.allocated_regions > 0);
		idx_to_remove = *rc::gen::inRange<std::size_t>(0, m.allocated_regions);
	}

	void apply(regions_model &m) const override
	{
		m.allocated_regions--;
	}

	void run(const regions_model &m, regions_test &s) const override
	{
		auto region = s.sut.helpers.get_nth_region(idx_to_remove);
		auto ret = s.sut.sut.region_free(region);
		RC_ASSERT(ret == 0);
	}
};

struct rc_failed_remove_region : regions_command {
	void run(const regions_model &m, regions_test &s) const override
	{
		struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};
		auto ret = s.sut.sut.region_free(invalid_region);
		RC_ASSERT(ret == -1);
	}
};

struct rc_iterate_regions : regions_command {
	void run(const regions_model &m, regions_test &s) const override
	{
		auto it = s.sut.sut.region_iterator();
		size_t regions_count = 0;
		struct pmemstream_region region;
		while (pmemstream_region_iterator_next(it.get(), &region) != -1) {
			regions_count++;
		}
		RC_ASSERT(regions_count == m.allocated_regions);
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
		ret += rc::check("Adding and removing multiple regions should be iterable and accesible",
				 [](pmemstream_empty &&stream) {
					 struct regions_model model;
					 struct regions_test sut(stream);

					 rc::state::check(model, sut,
							  rc::state::gen::execOneOfWithArgs<
								  rc_add_region, rc_failed_add_region, rc_remove_region,
								  rc_failed_remove_region, rc_iterate_regions>());
				 });
	});
}
