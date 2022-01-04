// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * reserve_publish.cpp -- pmemstream_reserve and pmemstream_publish functional test.
 *			It checks if we can reserve space for entry, write to that space, and persist it.
 *			It's executed among "regular" appends to confirm we can mix these up.
 */

#include <cstring>
#include <vector>

#include "libpmemstream_internal.h"
#include "unittest.hpp"

/* use bigger than default stream size */
static constexpr size_t STREAM_SIZE = 1024ULL * TEST_DEFAULT_STREAM_SIZE;

namespace
{
/* XXX: these helper methods are copy-pasted from "append_break.cpp" test. */

/* append all data in the vector at the offset (pointed by region_runtime) */
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
void reserve_and_publish(struct pmemstream *stream, struct pmemstream_region region,
			 const std::vector<std::string> &data_to_write)
{
	for (const auto &d : data_to_write) {
		/* reserve space for given data */
		struct pmemstream_entry reserved_entry;
		void *reserved_data;
		int ret = pmemstream_reserve(stream, region, nullptr, d.size(), &reserved_entry, &reserved_data);
		UT_ASSERTeq(ret, 0);

		/* write into the reserved space and publish (persist) it */
		memcpy(reserved_data, d.data(), d.size());

		/* XXX: add tests as well for non-temporal memcpy and no persist */
		ret = pmemstream_publish(stream, region, d.data(), d.size(), &reserved_entry);
		UT_ASSERTeq(ret, 0);
	}
}

/* initialize stream with a single region and initial data (if given) */
struct pmemstream_region init_stream_single_region(struct pmemstream *stream, size_t region_size,
						   const std::vector<std::string> data)
{
	struct pmemstream_region new_region;
	UT_ASSERTeq(pmemstream_region_allocate(stream, region_size, &new_region), 0);

	struct pmemstream_entry_iterator *eiter;
	UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream, new_region), 0);
	pmemstream_entry_iterator_delete(&eiter);

	append(stream, new_region, nullptr, data);

	return new_region;
}

/* read all elements in a region */
std::vector<std::string> get_elements_in_region(struct pmemstream *stream, struct pmemstream_region *region)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream, *region), 0);

	struct pmemstream_entry entry;
	while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
		auto data = reinterpret_cast<const char *>(pmemstream_entry_data(stream, entry));
		auto data_len = pmemstream_entry_length(stream, entry);
		result.emplace_back(data, data_len);
	}

	pmemstream_entry_iterator_delete(&eiter);

	return result;
}
} /* namespace */

static void test(int argc, char *argv[])
{
	if (argc != 2) {
		UT_FATAL("usage: %s file-path", argv[0]);
	}

	auto path = std::string(argv[1]);

	std::vector<std::string> init_data;
	init_data.emplace_back("1");
	init_data.emplace_back("32");
	init_data.emplace_back("1048576");
	/* XXX: switch to RC test and make use of generated data instead */

	/* initialize stream with a single region and "regularly" append initial data */
	auto s = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, STREAM_SIZE);
	auto r = init_stream_single_region(s.get(), TEST_DEFAULT_REGION_SIZE, init_data);

	std::vector<std::string> data_to_reserve;
	data_to_reserve.emplace_back("0");
	data_to_reserve.emplace_back("1234");
	data_to_reserve.emplace_back("ABCDEFGHIJKL");
	data_to_reserve.emplace_back(512, 'A');

	/* reserve and publish */
	/* XXX: make this work with property based testing - should work with regular append as well */
	reserve_and_publish(s.get(), r, data_to_reserve);

	/* add one more "regular" append */
	std::string extra_entry(1024, 'Z');
	int ret = pmemstream_append(s.get(), r, nullptr, extra_entry.data(), extra_entry.size(), nullptr);
	UT_ASSERTeq(ret, 0);

	/* verify count of all appended/written entries */
	/* XXX: make use of verify() in stream_helpers */
	auto read_elements = get_elements_in_region(s.get(), &r);
	auto cnt = read_elements.size();
	auto expected_cnt = init_data.size() + data_to_reserve.size() + 1;
	UT_ASSERTeq(cnt, expected_cnt);

	/* put all these entries into one vector and check if all data is written correctly */
	init_data.insert(init_data.end(), data_to_reserve.begin(), data_to_reserve.end());
	init_data.emplace_back(extra_entry);
	for (size_t i = 0; i < cnt; ++i) {
		UT_ASSERT(init_data[i] == read_elements[i]);
	}
}

int main(int argc, char *argv[])
{
	return run_test([&] { test(argc, argv); });
}
