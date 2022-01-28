// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "unittest.h"
#include <string.h>

/**
 * reserve_and_publish - unit test for pmemstream_reserve, pmemstream_publish
 */

struct entry_data {
	uint64_t data;
};

void test_reserve_and_publish(char *path)
{
	int ret;
	void *data_address = NULL;
	struct entry_data data;
	struct pmemstream_entry entry;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(stream, region, NULL, sizeof(data), &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	ret = pmemstream_publish(stream, region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void test_reserve_and_publish_with_memcpy(char *path)
{
	int ret;
	void *data_address = NULL;
	struct entry_data data;
	struct pmemstream_entry entry;
	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(stream, region, NULL, sizeof(data), &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	memcpy(&data_address, &data, sizeof(data));

	ret = pmemstream_publish(stream, region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, 0);

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

	test_reserve_and_publish(path);
	test_reserve_and_publish_with_memcpy(path);

	return 0;
}
