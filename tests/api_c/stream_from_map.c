// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "unittest.h"

/**
 * stream_from_map - unit test for pmemstream_from_map
 */

void test_stream_from_map(char *path, size_t file_size, size_t blk_size)
{
	struct pmem2_map *map = map_open(path, file_size, true);
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

	START();
	char *path = argv[1];

	test_stream_from_map(path, 4096 * 1024, 4096);
	test_stream_from_map(path, 10240, 64);

	return 0;
}
