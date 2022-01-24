// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"

/**
 * append_entry - unit test for pmemstream_append, pmemstream_entry_data,
 *					pmemstream_entry_length
 */

struct entry_data {
	uint64_t data;
};

void test_entry_append(char *path)
{
	int ret;
	size_t entry_size;
    struct entry_data data;
	const struct entry_data *entry_data;
	struct pmemstream_entry entry;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(&region, NULL);

	ret = pmemstream_append(stream, region, NULL, &data, sizeof(data), &entry);
    UT_ASSERTeq(ret, 0);
    UT_ASSERTne(&entry, NULL);

	entry_data = pmemstream_entry_data(stream, entry);
	UT_ASSERTne(entry_data, NULL);

	entry_size = pmemstream_entry_length(stream, entry);
	UT_ASSERTne(entry_size, 0);

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

	test_entry_append(path);

	return 0;
}
