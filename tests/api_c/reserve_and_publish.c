// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "stream_helpers.h"
#include "unittest.h"
#include <string.h>

/**
 * reserve_and_publish - unit test for pmemstream_reserve, pmemstream_publish
 */

struct entry_data {
	uint64_t data;
};

void valid_input_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	void *data_address = NULL;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_entry entry;
	struct pmemstream_region region;

	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(env.stream, region, NULL, sizeof(data), &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	ret = pmemstream_publish(env.stream, region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void valid_input_test_with_memcpy(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	void *data_address = NULL;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_entry entry;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(env.stream, region, NULL, sizeof(data), &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	memcpy(&data_address, &data, sizeof(data));

	ret = pmemstream_publish(env.stream, region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void null_stream_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	void *data_address = NULL;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_entry entry;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(NULL, region, NULL, sizeof(data), &entry, &data_address);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(data_address, NULL);

	ret = pmemstream_publish(NULL, region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, -1);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void invalid_region_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	void *data_address = NULL;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_entry entry;

	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	int ret = pmemstream_reserve(env.stream, invalid_region, NULL, sizeof(data), &entry, &data_address);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(data_address, NULL);

	ret = pmemstream_publish(env.stream, invalid_region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, -1);

	pmemstream_test_teardown(env);
}

void null_data_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	void *data_address = NULL;
	struct entry_data *data = NULL;
	struct pmemstream_entry entry;

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(env.stream, region, NULL, 0, &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	ret = pmemstream_publish(env.stream, region, NULL, data, 0, entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void zero_size_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	void *data_address = NULL;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_entry entry;

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(env.stream, region, NULL, 0, &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	ret = pmemstream_publish(env.stream, region, NULL, &data, 0, entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void null_entry_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	void *data_address = NULL;

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_reserve(env.stream, region, NULL, sizeof(struct entry_data), NULL, &data_address);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(data_address, NULL);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();

	char *path = argv[1];

	valid_input_test(path);
	valid_input_test_with_memcpy(path);
	null_stream_test(path);
	invalid_region_test(path);
	null_data_test(path);
	zero_size_test(path);
	null_entry_test(path);

	return 0;
}
