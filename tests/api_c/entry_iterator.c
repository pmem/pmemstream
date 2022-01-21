// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * entry_iterator - unit test for pmemstream_entry_iterator_new,
 *					pmemstream_entry_iterator_next, pmemstream_entry_iterator_delete
 */

void test_entry_iterator(char *path)
{
	int ret;	
	struct pmemstream_entry entry;
	struct pmemstream_entry_iterator *eiter;
	
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);	
	
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(&region, NULL);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);

	do
	{
		UT_ASSERTeq(ret, 0);
		ret = pmemstream_entry_iterator_next(eiter, &region, &entry);
		
	} while (ret == 0);	

	pmemstream_entry_iterator_delete(&eiter);
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

	test_entry_iterator(path);

	return 0;
}
