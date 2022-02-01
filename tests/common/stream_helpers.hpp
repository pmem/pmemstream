// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_STREAM_HELPERS_HPP
#define LIBPMEMSTREAM_STREAM_HELPERS_HPP

#include <algorithm>
#include <cstring>
#include <vector>

#include "unittest.hpp"

void append(struct pmemstream *stream, struct pmemstream_region region,
	    struct pmemstream_region_runtime *region_runtime, const std::vector<std::string> &data)
{
	for (const auto &e : data) {
		auto ret = pmemstream_append(stream, region, region_runtime, e.data(), e.size(), nullptr);
		UT_ASSERT(ret == 0);
	}
}

struct pmemstream_region initialize_stream_single_region(struct pmemstream *stream, size_t region_size,
							 const std::vector<std::string> &data)
{
	struct pmemstream_region new_region;
	UT_ASSERT(pmemstream_region_allocate(stream, region_size, &new_region) == 0);
	/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
	UT_ASSERT(pmemstream_region_size(stream, new_region) >= region_size);

	append(stream, new_region, NULL, data);

	return new_region;
}

std::vector<std::string> get_elements_in_region(struct pmemstream *stream, struct pmemstream_region region)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	UT_ASSERT(pmemstream_entry_iterator_new(&eiter, stream, region) == 0);

	struct pmemstream_entry entry;
	struct pmemstream_region r;
	while (pmemstream_entry_iterator_next(eiter, &r, &entry) == 0) {
		UT_ASSERT(r.offset == region.offset);
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

	UT_ASSERT(std::equal(all_elements.begin(), extra_data_start, data.begin()));
	UT_ASSERT(std::equal(extra_data_start, all_elements.end(), extra_data.begin()));
}

/* Reserve space, write data, and publish (persist) them, within the given region.
 * Do this for all data in the vector. */
void reserve_and_publish(struct pmemstream *stream, struct pmemstream_region region,
			 const std::vector<std::string> &data_to_write)
{
	pmemstream_region_runtime *runtime = nullptr;
	if (*rc::gen::arbitrary<bool>()) {
		int ret = pmemstream_get_region_runtime(stream, region, &runtime);
		UT_ASSERT(ret == 0);
	}

	for (const auto &d : data_to_write) {
		/* reserve space for given data */
		struct pmemstream_entry reserved_entry;
		void *reserved_data;
		int ret = pmemstream_reserve(stream, region, nullptr, d.size(), &reserved_entry, &reserved_data);
		UT_ASSERT(ret == 0);

		/* write into the reserved space and publish (persist) it */
		memcpy(reserved_data, d.data(), d.size());

		ret = pmemstream_publish(stream, region, runtime, d.data(), d.size(), &reserved_entry);
		UT_ASSERT(ret == 0);
	}
}
#endif /* LIBPMEMSTREAM_STREAM_HELPERS_HPP */
