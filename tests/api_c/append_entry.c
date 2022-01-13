// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "unittest.h"

/**
 * append_entry - unit test for pmemstream_append, pmemstream_entry_data,
 *					pmemstream_entry_length
 */

struct entry_data {
	uint64_t data;
};

void test_append_entry(char *path)
{
	int ret;
	struct entry_data data;
	data.data = UINT64_MAX;
	const struct entry_data *entry_data;
	struct pmemstream_entry entry;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(stream, region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, 0);

	entry_data = pmemstream_entry_data(stream, entry);
	UT_ASSERTne(entry_data, NULL);
	UT_ASSERTeq(entry_data->data, data.data);

	UT_ASSERTeq(pmemstream_entry_length(stream, entry), sizeof(data));

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void invalid_region_test(char *path)
{
	int ret;
	struct entry_data data;
	data.data = UINT64_MAX;
	struct pmemstream_entry *entry = NULL;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	ret = pmemstream_append(stream, invalid_region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(entry, NULL);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void invalid_entry_test(char *path)
{
	int ret;
	struct entry_data data = {.data = PTRDIFF_MAX};
	const struct entry_data *data_ptr = &data;
	struct pmemstream_entry invalid_entry = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	data_ptr = pmemstream_entry_data(stream, invalid_entry);
	UT_ASSERTeq(data_ptr, NULL);

	UT_ASSERTeq(pmemstream_entry_length(stream, invalid_entry), 0);

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

	test_append_entry(path);
	invalid_region_test(path);
	invalid_entry_test(path);

	return 0;
}
