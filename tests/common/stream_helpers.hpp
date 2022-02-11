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
		UT_ASSERTeq(ret, 0);
	}
}

/* Reserve space, write data, and publish (persist) them, within the given region.
 * Do this for all data in the vector. */
void reserve_and_publish(struct pmemstream *stream, struct pmemstream_region region, bool is_runtime_initialized,
			 const std::vector<std::string> &data)
{
	pmemstream_region_runtime *runtime = nullptr;
	if (!is_runtime_initialized) {
		int ret = pmemstream_region_runtime_initialize(stream, region, &runtime);
		UT_ASSERTeq(ret, 0);
	}

	for (const auto &d : data) {
		/* reserve space for given data */
		struct pmemstream_entry reserved_entry;
		void *reserved_data;
		int ret = pmemstream_reserve(stream, region, nullptr, d.size(), &reserved_entry, &reserved_data);
		UT_ASSERTeq(ret, 0);

		/* write into the reserved space and publish (persist) it */
		memcpy(reserved_data, d.data(), d.size());

		ret = pmemstream_publish(stream, region, runtime, d.data(), d.size(), &reserved_entry);
		UT_ASSERTeq(ret, 0);
	}
}

struct pmemstream_region initialize_stream_single_region(struct pmemstream *stream, size_t region_size,
							 const std::vector<std::string> &data)
{
	struct pmemstream_region new_region;
	UT_ASSERTeq(pmemstream_region_allocate(stream, region_size, &new_region), 0);
	/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
	UT_ASSERT(pmemstream_region_size(stream, new_region) >= region_size);

	append(stream, new_region, NULL, data);

	return new_region;
}

struct pmemstream_region get_first_region(struct pmemstream *stream)
{
	struct pmemstream_region_iterator *riter;
	int ret = pmemstream_region_iterator_new(&riter, stream);
	UT_ASSERTne(ret, -1);

	struct pmemstream_region region;
	ret = pmemstream_region_iterator_next(riter, &region);
	UT_ASSERTne(ret, -1);
	pmemstream_region_iterator_delete(&riter);

	return region;
}

struct pmemstream_entry get_last_entry(pmemstream *stream, pmemstream_region region)
{
	struct pmemstream_entry_iterator *eiter;
	UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream, region), 0);

	struct pmemstream_entry last_entry = {0};
	struct pmemstream_entry tmp_entry;
	while (pmemstream_entry_iterator_next(eiter, nullptr, &tmp_entry) == 0) {
		last_entry = tmp_entry;
	}

	if (last_entry.offset == 0)
		throw std::runtime_error("No elements in the stream");

	pmemstream_entry_iterator_delete(&eiter);

	return last_entry;
}

std::vector<std::string> get_elements_in_region(struct pmemstream *stream, struct pmemstream_region region)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream, region), 0);

	struct pmemstream_entry entry;
	struct pmemstream_region r;
	while (pmemstream_entry_iterator_next(eiter, &r, &entry) == 0) {
		UT_ASSERTeq(r.offset, region.offset);
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

#endif /* LIBPMEMSTREAM_STREAM_HELPERS_HPP */
