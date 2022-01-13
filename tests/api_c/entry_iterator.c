// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * entry_iterator - unit test for pmemstream_entry_iterator_new,
 *                  pmemstream_entry_iterator_next, pmemstream_entry_iterator_delete
 */

#define SIZE 1024

void test_entry_iterator(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	int s;

	s = pmemstream_entry_iterator_new(&eiter, stream, *region);
	UT_ASSERT(s == 0 || s == -1);
	UT_ASSERTne(eiter, NULL);

	s = pmemstream_entry_iterator_next(eiter, NULL, &entry);
	UT_ASSERT(s == 0 || s == -1);
	UT_ASSERTne(eiter, NULL);

	pmemstream_entry_iterator_delete(&eiter);
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

	test_entry_iterator(NULL, NULL);
	test_entry_iterator(stream, NULL);
	test_entry_iterator(NULL, region);
	test_entry_iterator(stream, region);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
