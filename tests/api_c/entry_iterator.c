// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "unittest.h"

/**
 * entry_iterator - unit test for pmemstream_entry_iterator_new,
 *					pmemstream_entry_iterator_next, pmemstream_entry_iterator_delete
 */

struct entry_data {
	uint64_t data;
};

void valid_input_test(char *path)
{
	int ret;
	struct pmemstream_entry entry;
	struct pmemstream_entry_iterator *eiter;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);

	ret = pmemstream_entry_iterator_next(eiter, &region, &entry);
	UT_ASSERTeq(ret, -1);

	pmemstream_entry_iterator_delete(&eiter);
	UT_ASSERTeq(eiter, NULL);
	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void test_get_last_entry(char *path)
{
	int ret;
	const int entries_count = 5;
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry last_entry;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	struct entry_data *e = malloc((uint64_t)entries_count * sizeof(struct entry_data));

	for (int i = 0; i < entries_count; i++) {
		e[i].data = (uint64_t)i;
		pmemstream_append(stream, region, NULL, &e[i], sizeof(e[i]), NULL);
	}

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);

	while (pmemstream_entry_iterator_next(eiter, &region, &last_entry) == 0) {
		/* NOP */
	}

	const struct entry_data *last_entry_data = pmemstream_entry_data(stream, last_entry);
	UT_ASSERTeq(last_entry_data->data, e[entries_count - 1].data);

	pmemstream_entry_iterator_delete(&eiter);
	UT_ASSERTeq(eiter, NULL);
	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
	free(e);
}

void invalid_region_test(char *path)
{
	int ret;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry entry;
	struct pmemstream_region region;
	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	// https://github.com/pmem/pmemstream/issues/99
	// ret = pmemstream_entry_iterator_new(&eiter, stream, invalid_region);
	// UT_ASSERTeq(ret, -1);
	// UT_ASSERTeq(eiter, NULL);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	ret = pmemstream_entry_iterator_next(eiter, &invalid_region, &entry);
	UT_ASSERTeq(ret, -1);

	pmemstream_entry_iterator_delete(&eiter);
	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void null_stream_test(char *path)
{
	int ret;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, NULL, region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void null_stream_and_invalid_region_test(char *path)
{
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};
	int ret;

	ret = pmemstream_entry_iterator_new(&eiter, NULL, invalid_region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);
}

void null_entry_iterator_test(char *path)
{
	int ret;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(NULL, stream, region);
	UT_ASSERTeq(ret, -1);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void null_entry_test(char *path)
{
	int ret;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_entry *entry = NULL;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_next(eiter, &region, entry);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(entry, NULL);

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

	valid_input_test(path);
	test_get_last_entry(path);
	valid_input_test(path);
	invalid_region_test(path);
	// https://github.com/pmem/pmemstream/issues/98
	// null_stream_test(path);
	// https://github.com/pmem/pmemstream/issues/98
	// https://github.com/pmem/pmemstream/issues/99
	// null_stream_and_invalid_region_test(path);
	// https://github.com/pmem/pmemstream/issues/137
	// null_entry_iterator_test(path);
	null_entry_test(path);

	return 0;
}
