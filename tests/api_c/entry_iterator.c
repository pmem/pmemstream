// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * entry_iterator - unit test for pmemstream_entry_iterator_new,
 *					pmemstream_entry_iterator_next, pmemstream_entry_iterator_delete
 */

void invalid_region_test(struct pmemstream *stream)
{
	int ret;
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	struct pmemstream_region region;
	struct pmemstream_region invalid_region = {.offset = UINT64_MAX};
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, stream, invalid_region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(&region, NULL);
	ret = pmemstream_entry_iterator_next(eiter, &invalid_region, &entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_entry_iterator_delete(&eiter);
	pmemstream_region_free(stream, invalid_region);
	pmemstream_region_free(stream, region);
}

void null_stream_test(struct pmemstream *stream)
{
	int ret;
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream *null_stream = NULL;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, null_stream, region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);

	pmemstream_region_free(stream, region);
}

void null_stream_and_invalid_region_test(struct pmemstream *stream)
{
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_region invalid_region = {.offset = UINT64_MAX};
	struct pmemstream *null_stream = NULL;
	int ret;

	ret = pmemstream_entry_iterator_new(&eiter, null_stream, invalid_region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);

	pmemstream_region_free(stream, invalid_region);
}

void null_entry_iterator_test(struct pmemstream *stream)
{
	int ret;
	struct pmemstream_entry_iterator *eiter = NULL;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);
}

void null_entry_test(struct pmemstream *stream)
{
	int ret;
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry *entry = NULL;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(&region, NULL);
	ret = pmemstream_entry_iterator_next(eiter, &region, entry);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(entry, NULL);

	pmemstream_entry_iterator_delete(&eiter);
	pmemstream_region_free(stream, region);
}

void valid_input_test(struct pmemstream *stream)
{
	int ret;
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);

	ret = pmemstream_entry_iterator_next(eiter, &region, &entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_entry_iterator_delete(&eiter);
	pmemstream_region_free(stream, region);
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

	invalid_region_test(stream);
	null_stream_test(stream);
	null_stream_and_invalid_region_test(stream);
	null_entry_iterator_test(stream);
	null_entry_test(stream);
	valid_input_test(stream);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
