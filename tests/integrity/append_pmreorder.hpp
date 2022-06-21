// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_APPEND_PMREORDER_HPP
#define LIBPMEMSTREAM_APPEND_PMREORDER_HPP

#include "libpmemstream_internal.h"
#include "random_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

#include <string>
#include <vector>

std::vector<std::vector<std::string>> generate_data(size_t regions, size_t entries)
{
	std::vector<std::vector<std::string>> result;
	for (size_t i = 0; i < regions; i++) {
		std::vector<std::string> region;
		for (size_t j = 0; j < entries; j++) {
			std::string value = std::to_string(rnd_generator());
			region.push_back(value);
		}
		result.push_back(region);
	}
	return result;
}

void fill(test_config_type test_config, size_t regions_count, size_t entries_in_region_count)
{
	pmemstream_with_multi_non_empty_regions(make_default_test_stream(),
						generate_data(regions_count, entries_in_region_count));
}

#endif /* LIBPMEMSTREAM_APPEND_PMREORDER_HPP */
