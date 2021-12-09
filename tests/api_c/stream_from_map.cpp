// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "unittest.hpp"

/**
 * stream_from_map - unit test for pmemstream_from_map in C++
 * Purpose of this test is to check if it's possible to compile and link with
 * pmemstream in C++. It may be removed when some more valuable tests are added.
 */

void test_stream_from_map(char *path, size_t file_size, size_t blk_size)
{
	struct pmem2_map *map = map_open(path, file_size);
	UT_ASSERTne(map, NULL);

	struct pmemstream *s = NULL;
	pmemstream_from_map(&s, blk_size, map);
	UT_ASSERTne(s, NULL);

	pmemstream_delete(&s);
	pmem2_map_delete(&map);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	char *path = argv[1];

	return run_test([&] { test_stream_from_map(path, 10240, 64); });
}
