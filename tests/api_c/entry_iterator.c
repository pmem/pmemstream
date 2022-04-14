// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "stream_helpers.h"
#include "unittest.h"

/**
 * entry_iterator - unit test for pmemstream_entry_iterator_new,
 *					pmemstream_entry_iterator_next, pmemstream_entry_iterator_delete
 */

#define ARBITRARY_VALUE 42

struct entry_data {
	uint64_t data;
};

void valid_input_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_entry_iterator *eiter;

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, env.stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);

	ret = pmemstream_entry_iterator_next(eiter);
	UT_ASSERTeq(ret, -1);

	pmemstream_entry_iterator_delete(&eiter);
	UT_ASSERTeq(eiter, NULL);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void test_get_last_entry(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	const uint64_t entries_count = 5;
	struct entry_data *entries = malloc(entries_count * sizeof(struct entry_data));
	UT_ASSERTne(entries, NULL);

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	for (uint64_t i = 0; i < entries_count; i++) {
		entries[i].data = i;
		ret = pmemstream_append(env.stream, region, NULL, &entries[i], sizeof(entries[i]), NULL);
		UT_ASSERTeq(ret, 0);
	}

	struct pmemstream_entry_iterator *eiter;
	ret = pmemstream_entry_iterator_new(&eiter, env.stream, region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(eiter, NULL);

	struct pmemstream_entry last_entry;
	do {
		pmemstream_entry_iterator_get(eiter, &last_entry);
	} while (pmemstream_entry_iterator_next(eiter) == 0);

	const struct entry_data *last_entry_data = pmemstream_entry_data(env.stream, last_entry);
	UT_ASSERTeq(last_entry_data->data, entries[entries_count - 1].data);

	pmemstream_entry_iterator_delete(&eiter);
	UT_ASSERTeq(eiter, NULL);
	pmemstream_region_free(env.stream, region);

	pmemstream_test_teardown(env);
	free(entries);
}

void null_iterator_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_entry entry = {.offset = ARBITRARY_VALUE};

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(NULL, env.stream, region);
	UT_ASSERTeq(ret, -1);

	ret = pmemstream_entry_iterator_next(NULL);
	UT_ASSERTeq(ret, -1);

	ret = pmemstream_entry_iterator_get(NULL, &entry);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(entry.offset, ARBITRARY_VALUE);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void invalid_region_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	const uint64_t invalid_offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes));
	struct pmemstream_entry_iterator *eiter = NULL;
	struct pmemstream_region region;
	struct pmemstream_region invalid_region = {.offset = invalid_offset};
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, env.stream, invalid_region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);

	pmemstream_entry_iterator_delete(&eiter);
	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void null_stream_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_entry_iterator *eiter = NULL;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, NULL, region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void null_stream_and_invalid_region_test(char *path)
{
	struct pmemstream_entry_iterator *eiter = NULL;
	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};
	int ret;

	ret = pmemstream_entry_iterator_new(&eiter, NULL, invalid_region);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(eiter, NULL);
}

void null_entry_iterator_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(NULL, env.stream, region);
	UT_ASSERTeq(ret, -1);

	struct pmemstream_entry entry;
	ret = pmemstream_entry_iterator_get(NULL, &entry);
	UT_ASSERTeq(ret, -1);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void null_entry_ptr_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_entry_iterator *eiter;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	size_t data = ARBITRARY_VALUE;
	ret = pmemstream_append(env.stream, region, NULL, &data, sizeof(data), NULL);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_new(&eiter, env.stream, region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_entry_iterator_get(eiter, NULL);
	UT_ASSERTeq(ret, -1);

	pmemstream_entry_iterator_delete(&eiter);
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
	test_get_last_entry(path);
	null_iterator_test(path);
	invalid_region_test(path);
	null_stream_test(path);
	null_stream_and_invalid_region_test(path);
	null_entry_iterator_test(path);
	null_entry_ptr_test(path);

	return 0;
}
