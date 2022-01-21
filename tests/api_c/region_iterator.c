// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * region_iterator - unit test for pmemstream_region_iterator_new,
 *					pmemstrem_region_iterator_next, pmemstream_region_iterator_delete
 */

void test_region_iterator(char *path)
{
	int ret;
	struct pmemstream_region_iterator *riter;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(&region, NULL);

	ret = pmemstream_region_iterator_new(&riter, stream);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(riter, NULL);

	ret = pmemstream_region_iterator_next(riter, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_region_iterator_next(riter, &region);
	UT_ASSERTeq(ret, -1);

	pmemstream_region_iterator_delete(&riter);
	UT_ASSERTeq(riter, NULL);
	pmemstream_region_free(stream, region);
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

	test_region_iterator(path);

	return 0;
}
