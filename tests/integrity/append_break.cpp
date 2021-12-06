// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * append_break.cpp -- pmemstream_append break - data integrity test
 */

#include "unittest.hpp"

#include <cstring>
#include <vector>

static constexpr size_t FILE_SIZE = 1024ULL * 1024 * 1024;
static constexpr size_t REGION_SIZE = FILE_SIZE - 16 * 1024;
static constexpr size_t BLK_SIZE = 4096;

namespace
{
/* XXX: these helper methods are copy-pasted from "append.cpp" RC test.
 *	We need to make them usable for all (RC and non-RC) tests (use defines/macros for ASSERTs?). */

/* append all data in the vector at the offset */
void append_at_offset(struct pmemstream *stream, struct pmemstream_region *region, struct pmemstream_entry *offset,
		      const std::vector<std::string> &data)
{
	for (const auto &e : data) {
		auto ret = pmemstream_append(stream, region, offset, e.data(), e.size(), nullptr);
		UT_ASSERTeq(ret, 0);
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

/* get first (and only) region */
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

/* get last offset (within a region) for next entry append */
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
		auto data_ptr = reinterpret_cast<char *>(pmemstream_entry_data(stream, entry));
		result.emplace_back(data_ptr, pmemstream_entry_length(stream, entry));
	}

	pmemstream_entry_iterator_delete(&eiter);

	return result;
}
} /* namespace */

static void test(int argc, char *argv[])
{
	if (argc != 3 || strchr("abi", argv[1][0]) == nullptr)
		UT_FATAL("usage: %s <a|b|i> file-name", argv[0]);

	const char *path = argv[2];
	std::vector<std::string> init_data;
	init_data.emplace_back("DEADBEEF");
	init_data.emplace_back("NONEMPTYDATA");
	init_data.emplace_back("mydata");

	try {
		if (argv[1][0] == 'a') {
			/* append initial data to a new stream */

			auto s = make_pmemstream(path, BLK_SIZE, FILE_SIZE); /* non-zero size to create a file */
			init_stream_single_region(s.get(), REGION_SIZE, &init_data);

		} else if (argv[1][0] == 'b') {
			/* break in the middle of an append */

			auto s = make_pmemstream(path, BLK_SIZE, 0);
			auto r = get_first_region(s.get());

			/* append (gdb script should tear the memcpy) */
			auto append_offset = get_append_offset(s.get(), &r);
			/* add entry longer than 512 */
			std::string buf(1024, '~');
			pmemstream_append(s.get(), &r, &append_offset, buf.data(), buf.size(), nullptr);

		} else if (argv[1][0] == 'i') {
			/* iterate all entries */

			auto s = make_pmemstream(path, BLK_SIZE, 0);
			auto r = get_first_region(s.get());

			/* read back data and count for the same output */
			auto read_elements = get_elements_in_region(s.get(), &r);
			/* While iterating over all entries, entry torn
			 * in the previous append should be cleared now. */
			auto cnt = read_elements.size();
			UT_ASSERTeq(cnt, init_data.size());
			for (size_t i = 0; i < cnt; ++i) {
				UT_ASSERT(init_data[i] == read_elements[i]);

				printf("%s\n", read_elements[i].c_str());
			}
		}
	} catch (...) {
		UT_FATAL("Something went wrong!");
	}
}

int main(int argc, char *argv[])
{
	return run_test([&] { test(argc, argv); });
}
