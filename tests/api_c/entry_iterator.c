// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * entry_iterator - unit test for pmemstream_entry_iterator_new,
 *					pmemstream_entry_iterator_next, pmemstream_entry_iterator_delete
 */

struct entry_data {
	uint64_t data;
};

void test_entry_iterator(char *path)
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
	const uint64_t entries_count = 5;
	struct entry_data *entries = malloc(entries_count * sizeof(struct entry_data));
	UT_ASSERTne(entries, NULL);

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	for (uint64_t i = 0; i < entries_count; i++) {
		entries[i].data = i;
		ret = pmemstream_append(stream, region, NULL, &entries[i], sizeof(entries[i]), NULL);
		UT_ASSERTeq(ret, 0);
	}

	struct pmemstream_entry_iterator *eiter;
	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);

	struct pmemstream_entry last_entry;
	while (pmemstream_entry_iterator_next(eiter, &region, &last_entry) == 0) {
		/* NOP */
	}

	const struct entry_data *last_entry_data = pmemstream_entry_data(stream, last_entry);
	UT_ASSERTeq(last_entry_data->data, entries[entries_count - 1].data);

	pmemstream_entry_iterator_delete(&eiter);
	UT_ASSERTeq(eiter, NULL);
	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
	free(entries);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();

	char *path = argv[1];

	test_entry_iterator(path);
	test_get_last_entry(path);

	return 0;
}
