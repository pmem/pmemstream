// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * region_iterator - unit test for pmemstream_region_iterator_new,
 *                   pmemstrem_region_iterator_next, pmemstream_region_iterator_delete
 */

#define SIZE 1024

void test_region_iterator(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_region_iterator *riter;
	int s;

	s = pmemstream_region_iterator_new(&riter, stream);
	UT_ASSERT(s == 0 || s == -1);
	UT_ASSERTne(riter, NULL);

	s = pmemstream_region_iterator_next(riter, &region);
	UT_ASSERT(s == 0 || s == -1);
	UT_ASSERTne(riter, NULL);

	pmemstream_region_iterator_delete(&riter);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();

	struct pmem2_map *map = map_open(argv[1], 10 * SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, 4 * SIZE, map);

	struct pmemstream_region *region;

	pmemstream_region_allocate(stream, SIZE, region);

	test_region_iterator(NULL, NULL);
	test_region_iterator(stream, NULL);
	test_region_iterator(NULL, region);
	test_region_iterator(stream, region);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
