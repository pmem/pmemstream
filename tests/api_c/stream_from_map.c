// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "libpmemstream_internal.h"
#include "unittest.h"

/**
 * stream_from_map - unit test for pmemstream_from_map and pmemstream_delete
 */

void test_stream_from_map(char *path, size_t file_size, size_t blk_size)
{
	struct pmem2_map *map = map_open(path, file_size, true);
	UT_ASSERTne(map, NULL);

	struct pmemstream *s = NULL;
	UT_ASSERTeq(pmemstream_from_map(&s, blk_size, map), 0);
	UT_ASSERTne(s, NULL);

	UT_ASSERTeq(pmemstream_committed_timestamp(s), PMEMSTREAM_INVALID_TIMESTAMP);
	UT_ASSERTeq(pmemstream_persisted_timestamp(s), PMEMSTREAM_INVALID_TIMESTAMP);

	pmemstream_delete(&s);
	pmem2_map_delete(&map);
}

void test_stream_from_map_invalid_size(char *path, size_t file_size, size_t blk_size)
{
	struct pmem2_map *map = map_open(path, file_size, true);
	UT_ASSERTne(map, NULL);

	struct pmemstream *s = NULL;
	UT_ASSERTne(pmemstream_from_map(&s, blk_size, map), 0);
	UT_ASSERTeq(s, NULL);

	pmem2_map_delete(&map);
}

void test_stream_from_map_null_map(char *path)
{
	struct pmemstream *s = NULL;
	UT_ASSERTne(pmemstream_from_map(&s, TEST_DEFAULT_BLOCK_SIZE, NULL), 0);
	UT_ASSERTeq(s, NULL);
}

void test_null_stream()
{
	UT_ASSERTeq(pmemstream_from_map(NULL, TEST_DEFAULT_BLOCK_SIZE, NULL), -1);
	pmemstream_delete(NULL);
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
	/* wrong block size*/
	test_stream_from_map_invalid_size(path, 10240, 0);
	/* wrong block size (not a power of 2) */
	test_stream_from_map_invalid_size(path, 10240, 10240);
	/* wrong block size (not a multiple of CACHELINE_SIZE) */
	test_stream_from_map_invalid_size(path, 1024, 8);
	/* usable stream size (after aligning down) is smaller than the block size */
	test_stream_from_map_invalid_size(path, 255, 64);
	/* usable stream size will not fit a single span_region */
	test_stream_from_map_invalid_size(path, 256, 64);
	/* test with map size to small for stream internal structures*/
	test_stream_from_map_invalid_size(path, 1, TEST_DEFAULT_BLOCK_SIZE);
	test_stream_from_map_null_map(path);
	test_null_stream();

	return 0;
}
