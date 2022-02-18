// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * append_break.cpp -- pmemstream_append break - data integrity test
 */

#include "stream_helpers.hpp"
#include "unittest.hpp"

#include <cstring>
#include <vector>

static void test(char mode)
{
	std::vector<std::string> init_data;
	init_data.emplace_back("DEADBEEF");
	init_data.emplace_back("NONEMPTYDATA");
	init_data.emplace_back("mydata");

	if (mode == 'a') {
		/* append initial data to a new stream */

		pmemstream_sut s(get_test_config().filename, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE);
		s.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, init_data);

	} else if (mode == 'b') {
		/* break in the middle of an append */

		pmemstream_sut s(get_test_config().filename, TEST_DEFAULT_BLOCK_SIZE, 0, false);
		auto r = s.helpers.get_first_region();

		/* append (gdb script should tear the memcpy) */
		/* add entry longer than 512 */
		std::string buf(1500, '~');
		s.append(r, buf);
		UT_ASSERT_UNREACHABLE;

	} else if (mode == 'i') {
		/* iterate all entries */

		pmemstream_sut s(get_test_config().filename, TEST_DEFAULT_BLOCK_SIZE, 0, false);
		auto r = s.helpers.get_first_region();

		/* read back data and count for the same output */
		auto read_elements = s.helpers.get_elements_in_region(r);
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
	if (argc != 3 || strchr("abi", argv[1][0]) == nullptr)
		UT_FATAL("usage: %s <a|b|i> file-path ", argv[0]);

	struct test_config_type test_config;
	test_config.filename = std::string(argv[2]);

	auto mode = argv[1][0];
	return run_test(test_config, [&] { test(mode); });
}
