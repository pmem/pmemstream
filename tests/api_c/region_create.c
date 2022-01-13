// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * region_create - unit test for pmemstream_region_allocate, pmemstream_region_free,
 * 					 pmemstream_region_size, pmemstream_get_region_runtime
 */

void null_size_test(char *path)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_STREAM_SIZE, map);
	size_t null_size = NULL;
	int s;

	struct pmemstream_region region;
	s = pmemstream_region_allocate(stream, null_size, &region);
	UT_ASSERT(s == 0);
	UT_ASSERTeq(&region, NULL);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void null_region_test(char *path)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_STREAM_SIZE, map);
	struct pmemstream_region null_region;
	int s;

	size_t s;
	s = pmemstream_region_size(stream, null_region);
	UT_ASSERTeq(s, NULL);

	struct pmemstream_region_runtime *rtm = NULL;
	s = pmemstream_get_region_runtime(stream, null_region, &rtm);
	UT_ASSERT(s == -1);
	UT_ASSERTeq(rtm, NULL);

	s = pmemstream_region_free(stream, null_region);
	UT_ASSERT(s == -1);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void valid_input_test(char *path, size_t size)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_STREAM_SIZE, map);
	int s;

	struct pmemstream_region region;
	s = pmemstream_region_allocate(stream, size, &region);
	UT_ASSERT(s == 0);
	UT_ASSERTne(&region, NULL);

	size_t s;
	s = pmemstream_region_size(stream, region);
	UT_ASSERTne(s, NULL);

	struct pmemstream_region_runtime *rtm = NULL;
	s = pmemstream_get_region_runtime(stream, region, &rtm);
	UT_ASSERT(s == 0);
	UT_ASSERTne(rtm, NULL);

	s = pmemstream_region_free(stream, region);
	UT_ASSERT(s == 0);

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

	null_size_test(path);
	null_region_test(path);
	valid_input_test(path, TEST_DEFAULT_REGION_SIZE);

	return 0;
}
