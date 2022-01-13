// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * region_allocate - unit test for pmemstream_region_allocate, pmemstream_region_free,
 * 					 pmemstream_region_size, pmemstream_get_region_runtime
 */

#define SIZE 1024

void test_pmemstream_region_allocation(char *path, size_t size)
{
	struct pmem2_map *map = map_open(path, 10 * SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, 4 * SIZE, map);
	int s;

	struct pmemstream_region region;
	s = pmemstream_region_allocate(stream, size, &region);
	UT_ASSERT(s == 0 || s == -1);
	UT_ASSERTne(&region, NULL);

	size_t s;
	s = pmemstream_region_size(stream, region);
	UT_ASSERTne(s, NULL);

	struct pmemstream_region_context *ctx = NULL;
	s = pmemstream_get_region_context(stream, region, &ctx);
	UT_ASSERT(s == 0 || s == -1);
	UT_ASSERTne(ctx, NULL);

	s = pmemstream_region_free(stream, region);
	UT_ASSERT(s == 0 || s == -1);

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

	test_stream_region_allocation(path, NULL);
	test_stream_region_allocation(path, 4 * SIZE);

	return 0;
}
