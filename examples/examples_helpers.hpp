// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * Helper, common functions for examples.
 * They aim to simplify examples' workflow. */

#ifndef LIBPMEMSTREAM_EXAMPLES_HELPERS_HPP
#define LIBPMEMSTREAM_EXAMPLES_HELPERS_HPP

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

template <typename Function>
void parallel_exec(size_t threads_number, Function f)
{
	std::vector<std::thread> threads;
	threads.reserve(threads_number);

	for (size_t i = 0; i < threads_number; ++i) {
		threads.emplace_back(
			[&](size_t id) {
				try {
					f(id);
				} catch (std::exception &e) {
					fprintf(stderr, "Error: %s occured in thread %ld\n", e.what(), id);
					exit(id);
				}
			},
			i);
	}

	for (auto &t : threads) {
		t.join();
	}
}

void initialize_stream(std::string path, struct pmem2_map **map, struct pmemstream **stream)
{
	*map = example_map_open(path.c_str(), EXAMPLE_STREAM_SIZE);
	assert(*map != NULL);

	int ret = pmemstream_from_map(stream, 4096, *map);
	assert(ret == 0);
	(void)ret;
}

std::vector<pmemstream_region> create_multiple_regions(struct pmemstream **stream, size_t number_of_regions,
						       size_t region_size)
{
	std::vector<pmemstream_region> regions;
	for (size_t i = 0; i < number_of_regions; i++) {
		struct pmemstream_region new_region;
		int ret = pmemstream_region_allocate(*stream, region_size, &new_region);
		if (ret != 0) {
			std::runtime_error("Cannot create regions");
		}
		regions.push_back(new_region);
	}
	return regions;
}

#endif /* LIBPMEMSTREAM_EXAMPLES_HELPERS_HPP */
