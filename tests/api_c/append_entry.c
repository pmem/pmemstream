// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "stream_helpers.h"
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
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct entry_data data;
	data.data = UINT64_MAX;
	const struct entry_data *entry_data;
	struct pmemstream_entry entry;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(env.stream, region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, 0);

	entry_data = pmemstream_entry_data(env.stream, entry);
	UT_ASSERTne(entry_data, NULL);
	UT_ASSERTeq(entry_data->data, data.data);

	UT_ASSERTeq(pmemstream_entry_length(env.stream, entry), sizeof(data));

	pmemstream_test_teardown(env);
}

void invalid_region_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct entry_data data;
	data.data = UINT64_MAX;
	struct pmemstream_entry *entry = NULL;

	struct pmemstream_region region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	ret = pmemstream_append(env.stream, region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(entry, NULL);

	pmemstream_test_teardown(env);
}

void null_region_runtime_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct entry_data data;
	data.data = UINT64_MAX;
	struct pmemstream_entry *entry = NULL;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(env.stream, region, NULL, &data, 0, entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_test_teardown(env);
}

void non_null_region_runtime_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct entry_data data;
	data.data = UINT64_MAX;
	struct pmemstream_entry *entry = NULL;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	struct pmemstream_region_runtime *region_runtime;
	ret = pmemstream_region_runtime_initialize(env.stream, region, &region_runtime);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(env.stream, region, region_runtime, &data, 0, entry);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_free(env.stream, region);

	pmemstream_test_teardown(env);
}

void null_data_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_entry *entry = NULL;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(env.stream, region, NULL, NULL, 0, entry);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(entry, NULL);

	pmemstream_region_free(env.stream, region);

	pmemstream_test_teardown(env);
}

void null_entry_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct entry_data data;
	data.data = PTRDIFF_MAX;
	struct pmemstream_entry *entry = NULL;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(env.stream, region, NULL, &data, sizeof(data), entry);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(entry, NULL);

	pmemstream_region_free(env.stream, region);

	pmemstream_test_teardown(env);
}

void invalid_entry_test(char *path)
{

	pmemstream_test_env env = pmemstream_test_make_default(path);
	struct entry_data e;
	e.data = PTRDIFF_MAX;
	const struct entry_data *entry_data = &e;
	struct pmemstream_entry entry = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	entry_data = pmemstream_entry_data(env.stream, entry);
	UT_ASSERTeq(entry_data, NULL);

	UT_ASSERTeq(pmemstream_entry_length(env.stream, entry), 0);

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
	// https://github.com/pmem/pmemstream/issues/99
	// invalid_region_test(path);
	null_region_runtime_test(path);
	non_null_region_runtime_test(path);
	null_data_test(path);
	null_entry_test(path);
	// https://github.com/pmem/pmemstream/issues/100
	// invalid_entry_test(path);

	return 0;
}
