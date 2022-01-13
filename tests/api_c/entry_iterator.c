// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * entry_iterator - unit test for pmemstream_entry_iterator_new,
 *                  pmemstream_entry_iterator_next, pmemstream_entry_iterator_delete
 */

void null_region_test(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	struct pmemstream_region *null_region = NULL;
	int s;

	s = pmemstream_entry_iterator_new(&eiter, stream, *null_region);
	UT_ASSERT(s == -1);
	UT_ASSERTeq(eiter, NULL);

	pmemstream_entry_iterator_new(&eiter, stream, *region);
	s = pmemstream_entry_iterator_next(eiter, null_region, &entry);
	UT_ASSERT(s == -1);

	pmemstream_entry_iterator_delete(&eiter);
}

void null_stream_test(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	struct pmemstream *null_stream = NULL;
	int s;

	s = pmemstream_entry_iterator_new(&eiter, null_stream, *region);
	UT_ASSERT(s == -1);
	UT_ASSERTeq(eiter, NULL);
}

void null_stream_and_region_test(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	struct pmemstream_region *null_region = NULL;
	struct pmemstream *null_stream = NULL;
	int s;

	s = pmemstream_entry_iterator_new(&eiter, null_stream, *null_region);
	UT_ASSERT(s == -1);
	UT_ASSERTeq(eiter, NULL);
}

void valid_input_test(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	int s;

	s = pmemstream_entry_iterator_new(&eiter, stream, *region);
	UT_ASSERT(s == 0);
	UT_ASSERTne(eiter, NULL);

	s = pmemstream_entry_iterator_next(eiter, region, &entry);
	UT_ASSERT(s == 0);

	pmemstream_entry_iterator_delete(&eiter);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();

	struct pmem2_map *map = map_open(argv[1], TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_STREAM_SIZE, map);

	struct pmemstream_region *region;
	pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, region);

	null_region_test(stream, region);
	null_stream_test(stream, region);
	null_stream_and_region_test(stream, region);
	valid_input_test(stream, region);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
