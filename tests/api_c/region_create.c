// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * region_create - unit test for pmemstream_region_allocate, pmemstream_region_free,
 *					pmemstream_region_size, pmemstream_region_runtime_initialize
 */

void test_region_create(char *path)
{
	int ret;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	size_t region_size;
	region_size = pmemstream_region_size(stream, region);
	UT_ASSERT(region_size >= TEST_DEFAULT_REGION_SIZE);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_region_runtime_initialize(stream, region, &rtm);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(rtm, NULL);

	ret = pmemstream_region_free(stream, region);
	UT_ASSERTeq(ret, 0);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();
	char *path = argv[1];

	test_region_create(path);

	return 0;
}
