// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <rapidcheck.h>

#include "iterator.h"
#include "libpmemstream.h"
#include "pmemstream_runtime.h"
#include "stream_helpers.hpp"
#include "unittest.hpp"

int count_regions(pmemstream_test_base &stream)
{
	auto riter = stream.helpers.stream.region_iterator();

	int region_counter = 0;
	struct pmemstream_region region;
	while (pmemstream_region_iterator_next(riter.get(), &region) != -1) {
		UT_ASSERTne(region.offset, PMEMSTREAM_INVALID_OFFSET);
		++region_counter;
	}
	return region_counter;
}

void remove_regions(pmemstream_test_base &stream, int number)
{
	struct pmemstream_region_iterator *riter;
	for (int i = 0; i < number; i++) {
		int ret = pmemstream_region_iterator_new(&riter, stream.sut.c_ptr());
		UT_ASSERTeq(ret, 0);
		pmemstream_region_free(stream.sut.c_ptr(), riter->region);
		UT_ASSERTeq(ret, 0);
		pmemstream_region_iterator_delete(&riter);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}

	struct test_config_type test_config;
	test_config.filename = std::string(argv[1]);
	test_config.block_size = 64;

	return run_test(test_config, [&] {
		return_check ret;
		size_t max_allocations = 0;
		size_t allocation_size = test_config.block_size * 100;

		{
			pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
						    get_test_config().stream_size);
			std::tuple<int, pmemstream_region> region;
			do {
				region = stream.helpers.stream.region_allocate(allocation_size);
				++max_allocations;
			} while (std::get<0>(region) != -1);
		}
		ret += rc::check("Multiple allocate", [&]() {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);
			pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
						    get_test_config().stream_size);

			for (size_t i = 0; i < no_regions; i++) {
				auto region = stream.helpers.stream.region_allocate(allocation_size);
				UT_ASSERTeq(std::get<0>(region), 0);
			}

			UT_ASSERTeq(no_regions, static_cast<size_t>(count_regions(stream)));
		});

		ret += rc::check("Multiple allocate - multiple free", [&]() {
			size_t no_regions = *rc::gen::inRange<std::size_t>(1, max_allocations);
			size_t to_delete = *rc::gen::inRange<std::size_t>(1, no_regions);
			pmemstream_test_base stream(get_test_config().filename, get_test_config().block_size,
						    get_test_config().stream_size);

			for (size_t i = 0; i < no_regions; i++) {
				auto region = stream.helpers.stream.region_allocate(allocation_size);
				UT_ASSERTeq(std::get<0>(region), 0);
			}

			UT_ASSERTeq(no_regions, static_cast<size_t>(count_regions(stream)));

			remove_regions(stream, to_delete);
			UT_ASSERTeq(no_regions - to_delete, static_cast<size_t>(count_regions(stream)));
		});
	});
}
