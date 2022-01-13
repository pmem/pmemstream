// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "span.h"
#include "unittest.h"

/**
 * append_entry - unit test for pmemstream_append, pmemstream_entry_data,
 *					pmemstream_entry_length
 */

struct entry_data {
	uint64_t data;
};

void valid_input_test(char *path)
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

	struct pmemstream_region region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	ret = pmemstream_append(stream, region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(entry, NULL);

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void null_data_test(char *path)
{
	int ret;
	struct pmemstream_entry *entry = NULL;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(stream, region, NULL, NULL, 0, entry);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(entry, NULL);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void null_entry_test(char *path)
{
	int ret;
	struct entry_data data;
	data.data = PTRDIFF_MAX;
	struct pmemstream_entry *entry = NULL;

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(stream, region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(entry, NULL);

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);
}

void invalid_entry_test(char *path)
{
	int ret;
	const struct entry_data *entry_data;
	struct pmemstream_entry entry = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	struct pmem2_map *map = map_open(path, TEST_DEFAULT_STREAM_SIZE, true);
	struct pmemstream *stream;
	ret = pmemstream_from_map(&stream, TEST_DEFAULT_BLOCK_SIZE, map);
	UT_ASSERTeq(ret, 0);

	entry_data = pmemstream_entry_data(stream, entry);
	UT_ASSERTeq(entry_data, NULL);

	UT_ASSERTeq(pmemstream_entry_length(stream, entry), 0);

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
	// https://github.com/pmem/pmemstream/issues/99
	// invalid_region_test(path);
	null_data_test(path);
	null_entry_test(path);
	// https://github.com/pmem/pmemstream/issues/100
	// invalid_entry_test(path);

	return 0;
}
