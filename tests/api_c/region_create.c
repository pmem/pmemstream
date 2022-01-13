// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * region_create - unit test for pmemstream_region_allocate, pmemstream_region_free,
 *					pmemstream_region_size, pmemstream_get_region_runtime
 */

void null_size_test(char *path)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_STREAM_SIZE, map);
	size_t zero_size = 0;
	int ret;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, zero_size, &region);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void invalid_region_test(char *path)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_STREAM_SIZE, map);
	struct pmemstream_region invalid_region = {.offset = UINT64_MAX};
	int ret;

	size_t size;
	size = pmemstream_region_size(stream, invalid_region);
	UT_ASSERTeq(size, 0);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_get_region_runtime(stream, invalid_region, &rtm);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(rtm, NULL);

	ret = pmemstream_region_free(stream, invalid_region);
	UT_ASSERTeq(ret, -1);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void valid_input_test(char *path, size_t size)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_STREAM_SIZE, map);
	int ret;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, size, &region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(&region, NULL);

	size_t region_size;
	region_size = pmemstream_region_size(stream, region);
	UT_ASSERTne(region_size, 0);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_get_region_runtime(stream, region, &rtm);
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

	null_size_test(path);
	invalid_region_test(path);
	valid_input_test(path, TEST_DEFAULT_REGION_SIZE);

	return 0;
}
