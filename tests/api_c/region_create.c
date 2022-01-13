// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "unittest.h"

/**
 * region_create - unit test for pmemstream_region_allocate, pmemstream_region_free,
 *					pmemstream_region_size, pmemstream_region_runtime_initialize
 */

void valid_input_test(char *path)
{
	int ret;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	UT_ASSERT(pmemstream_region_size(stream, region) >= TEST_DEFAULT_REGION_SIZE);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_region_runtime_initialize(stream, region, &rtm);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(rtm, NULL);

	ret = pmemstream_region_free(stream, region);
	UT_ASSERTeq(ret, 0);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void null_stream_test(char *path)
{
	int ret;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq(pmemstream_region_size(NULL, region), 0);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_region_runtime_initialize(NULL, region, &rtm);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(rtm, NULL);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void zero_size_test(char *path)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	int ret;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, 0, &region);
	UT_ASSERTeq(ret, -1);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void invalid_region_test(char *path)
{
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};
	int ret;

	UT_ASSERT(pmemstream_region_size(stream, invalid_region) == 0);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_region_runtime_initialize(stream, invalid_region, &rtm);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(rtm, NULL);

	ret = pmemstream_region_free(stream, invalid_region);
	UT_ASSERTeq(ret, -1);

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

	valid_input_test(path);
	null_stream_test(path);
	zero_size_test(path);
	invalid_region_test(path);

	return 0;
}
