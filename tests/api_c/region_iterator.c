// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * region_iterator - unit test for pmemstream_region_iterator_new,
 *					pmemstrem_region_iterator_next, pmemstream_region_iterator_delete
 */

void invalid_region_test(struct pmemstream *stream)
{
	struct pmemstream_region_iterator *riter;
	struct pmemstream_region invalid_region = {.offset = UINT64_MAX};
	int ret;

	ret = pmemstream_region_iterator_new(&riter, stream);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(riter, NULL);

	ret = pmemstream_region_iterator_next(riter, &invalid_region);
	UT_ASSERTeq(ret, -1);

	pmemstream_region_iterator_delete(&riter);
	pmemstream_region_free(stream, invalid_region);
}

void null_stream_test()
{
	struct pmemstream_region_iterator *riter;
	struct pmemstream *null_stream = NULL;
	int ret;

	ret = pmemstream_region_iterator_new(&riter, null_stream);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(riter, NULL);
}

void valid_input_test(struct pmemstream *stream, struct pmemstream_region *region)
{
	struct pmemstream_region_iterator *riter;
	int ret;

	ret = pmemstream_region_iterator_new(&riter, stream);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(riter, NULL);

	ret = pmemstream_region_iterator_next(riter, region);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_iterator_delete(&riter);
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

	struct pmemstream_region region;
	pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);

	invalid_region_test(stream);
	null_stream_test();
	valid_input_test(stream, &region);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
