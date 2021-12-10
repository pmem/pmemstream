// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * reserve_publish.cpp -- pmemstream_reserve and pmemstream_publish functional test.
 *			It checks if we can reserve space for entry, write to that space, and persist it.
 *			It's executed among "regular" appends to confirm we can mix these up.
 */

#include "unittest.hpp"

#include <cstring>
#include <vector>

static constexpr size_t FILE_SIZE = 1024ULL * 1024 * 1024;
static constexpr size_t REGION_SIZE = FILE_SIZE - 16 * 1024;
static constexpr size_t BLK_SIZE = 4096;

namespace
{
/* XXX: these helper methods are copy-pasted from "append_break.cpp" test. */

/* append all data in the vector at the offset */
void append_at_offset(struct pmemstream *stream, struct pmemstream_region *region, struct pmemstream_entry *offset,
		      const std::vector<std::string> &data)
{
	for (const auto &e : data) {
		UT_ASSERTeq(pmemstream_append(stream, region, offset, e.data(), e.size(), nullptr), 0);
	}
}

/* Reserve space, write data, and publish (persist) them, within the given region, at the offset.
 * Do this for all data in the vector. */
void reserve_and_publish_at_offset(struct pmemstream *stream, struct pmemstream_region *region,
				   struct pmemstream_entry *entry, const std::vector<std::string> &data_to_write)
{
	for (const auto &d : data_to_write) {
		/* reserve space for given data */
		struct pmemstream_entry reserved_entry;
		uint8_t *reserved_data;
		UT_ASSERTeq(pmemstream_reserve(stream, region, entry, d.size(), &reserved_entry, &reserved_data), 0);

		/* write into the reserved space and publish (persist) it */
		memcpy(reserved_data, d.data(), d.size());
		UT_ASSERTeq(pmemstream_publish(stream, region, &reserved_entry, d.data(), d.size()), 0);
	}
}

/* initialize stream with a single region and initial data (if given) */
struct pmemstream_region init_stream_single_region(struct pmemstream *stream, size_t region_size,
						   const std::vector<std::string> *data = nullptr)
{
	struct pmemstream_region new_region;
	UT_ASSERTeq(pmemstream_region_allocate(stream, region_size, &new_region), 0);

	struct pmemstream_entry_iterator *eiter;
	UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream, new_region), 0);

	/* Find out offset for the first entry in region */
	struct pmemstream_entry entry;
	UT_ASSERTeq(pmemstream_entry_iterator_next(eiter, NULL, &entry), -1);
	pmemstream_entry_iterator_delete(&eiter);

	if (data) {
		append_at_offset(stream, &new_region, &entry, *data);
	}

	return new_region;
}

/* get last offset (within a region) for next entry append */
/* XXX: rename to "get_last/next_offset" ? */
struct pmemstream_entry get_append_offset(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_entry_iterator *eiter;
	UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream, *region), 0);

	struct pmemstream_entry entry;
	while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
		/* do nothing */
	}

	pmemstream_entry_iterator_delete(&eiter);

	return entry;
}

/* read all elements in a region */
std::vector<std::string> get_elements_in_region(struct pmemstream *stream, struct pmemstream_region *region)
{
	std::vector<std::string> result;

	struct pmemstream_entry_iterator *eiter;
	UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream, *region), 0);

	struct pmemstream_entry entry;
	while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
		auto data = reinterpret_cast<char *>(pmemstream_entry_data(stream, entry));
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
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	const char *path = argv[1];
	std::vector<std::string> init_data;
	init_data.emplace_back("1");
	init_data.emplace_back("32");
	init_data.emplace_back("1048576");
	/* XXX: switch to RC test and make use of generated data instead */

	try {
		/* initialize stream with a single region and "regularly" append initial data */
		auto s = make_pmemstream(path, BLK_SIZE, FILE_SIZE);
		auto r = init_stream_single_region(s.get(), REGION_SIZE, &init_data);
		auto last_offset = get_append_offset(s.get(), &r);

		std::vector<std::string> data_to_reserve;
		data_to_reserve.emplace_back("0");
		data_to_reserve.emplace_back("1234");
		data_to_reserve.emplace_back("ABCDEFGHIJKL");
		std::string long_entry(512, 'A');
		data_to_reserve.emplace_back(long_entry);

		/* reserve and publish at the last offset (and update the offset value) */
		reserve_and_publish_at_offset(s.get(), &r, &last_offset, data_to_reserve);

		/* add one more "regular" append */
		std::string extra_entry(1024, 'Z');
		UT_ASSERTeq(
			pmemstream_append(s.get(), &r, &last_offset, extra_entry.data(), extra_entry.size(), nullptr),
			0);

		/* verify count of all appended/written entries */
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
	} catch (...) {
		UT_FATAL("Something went wrong!");
	}
}

int main(int argc, char *argv[])
{
	return run_test([&] { test(argc, argv); });
}
