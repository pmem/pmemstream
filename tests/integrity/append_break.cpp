// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * append_break.cpp -- pmemstream_append break - data integrity test
 */

#include "stream_helpers.hpp"
#include "unittest.hpp"

#include <cstring>
#include <vector>

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
		initialize_stream_single_region(s.get(), TEST_DEFAULT_REGION_SIZE, init_data);

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
		auto read_elements = get_elements_in_region(s.get(), r);
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
