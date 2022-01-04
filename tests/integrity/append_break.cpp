// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * append_break.cpp -- pmemstream_append break - data integrity test
 */

#include "unittest.hpp"

#include <cstring>
#include <vector>

namespace
{
/* XXX: these helper methods are copy-pasted from "append.cpp" RC test.
 *	We need to make them usable for all (RC and non-RC) tests (use defines/macros for ASSERTs?). */

/* append all data in the vector at the offset */
void append(struct pmemstream *stream, struct pmemstream_region region, const std::vector<std::string> &data)
{
	for (const auto &e : data) {
		auto ret = pmemstream_append(stream, region, nullptr, e.data(), e.size(), nullptr);
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

	append(stream, new_region, *data);

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
		UT_FATAL("usage: %s <a|b|i> file-path ", argv[0]);

	auto path = std::string(argv[2]);

	std::vector<std::string> init_data;
	init_data.emplace_back("DEADBEEF");
	init_data.emplace_back("NONEMPTYDATA");
	init_data.emplace_back("mydata");

	if (argv[1][0] == 'a') {
		/* append initial data to a new stream */

		auto s = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
		init_stream_single_region(s.get(), TEST_DEFAULT_REGION_SIZE, &init_data);

	} else if (argv[1][0] == 'b') {
		/* break in the middle of an append */

		auto s = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, 0, false);
		auto r = get_first_region(s.get());

		/* append (gdb script should tear the memcpy) */
		/* add entry longer than 512 */
		std::string buf(1500, '~');
		pmemstream_append(s.get(), r, NULL, buf.data(), buf.size(), nullptr);
		UT_ASSERT_UNREACHABLE;

	} else if (argv[1][0] == 'i') {
		/* iterate all entries */

		auto s = make_pmemstream(path, TEST_DEFAULT_BLOCK_SIZE, 0, false);
		auto r = get_first_region(s.get());

		/* read back data and count for the same output */
		auto read_elements = get_elements_in_region(s.get(), &r);
		/* While iterating over all entries, entry torn
		 * in the previous append should be cleared now. */
		auto cnt = read_elements.size();
		UT_ASSERTeq(cnt, init_data.size());
		for (size_t i = 0; i < cnt; ++i) {
			UT_ASSERT(init_data[i] == read_elements[i]);
		}
	}
}

int main(int argc, char *argv[])
{
	return run_test([&] { test(argc, argv); });
}
