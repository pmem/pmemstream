// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_STREAM_HELPERS_HPP
#define LIBPMEMSTREAM_STREAM_HELPERS_HPP

#include "unittest.hpp"

#include <algorithm>
#include <vector>

void append(struct pmemstream *stream, struct pmemstream_region region,
	    struct pmemstream_region_runtime *region_runtime, const std::vector<std::string> &data)
{
	for (const auto &e : data) {
		auto ret = pmemstream_append(stream, region, region_runtime, e.data(), e.size(), nullptr);
		RC_ASSERT(ret == 0);
	}
}

struct pmemstream_region initialize_stream_single_region(struct pmemstream *stream, size_t region_size,
							 const std::vector<std::string> &data)
{
	struct pmemstream_region new_region;
	RC_ASSERT(pmemstream_region_allocate(stream, region_size, &new_region) == 0);

	append(stream, new_region, NULL, data);

	return new_region;
}

std::vector<std::string> get_elements_in_region(struct pmemstream *stream, struct pmemstream_region region)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	RC_ASSERT(pmemstream_entry_iterator_new(&eiter, stream, region) == 0);

	struct pmemstream_entry entry;
	while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
		auto data_ptr = reinterpret_cast<const char *>(pmemstream_entry_data(stream, entry));
		result.emplace_back(data_ptr, pmemstream_entry_length(stream, entry));
	}

	pmemstream_entry_iterator_delete(&eiter);

	return result;
}

/* XXX: extend to allow more than one extra_data vector */
void verify(pmemstream *stream, pmemstream_region region, const std::vector<std::string> &data,
	    const std::vector<std::string> &extra_data)
{
	/* Verify if stream now holds data + extra_data */
	auto all_elements = get_elements_in_region(stream, region);
	auto extra_data_start = all_elements.begin() + static_cast<int>(data.size());

	RC_ASSERT(std::equal(all_elements.begin(), extra_data_start, data.begin()));
	RC_ASSERT(std::equal(extra_data_start, all_elements.end(), extra_data.begin()));
}

#endif /* LIBPMEMSTREAM_STREAM_HELPERS_HPP */
