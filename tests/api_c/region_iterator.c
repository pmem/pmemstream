// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "stream_helpers.h"
#include "unittest.h"

/**
 * region_iterator - unit test for pmemstream_region_iterator_new,
 *					pmemstrem_region_iterator_next, pmemstream_region_iterator_delete
 */

void valid_input_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	struct pmemstream_region_iterator *riter;

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_region_iterator_new(&riter, env.stream);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(riter, NULL);

	pmemstream_region_iterator_seek_first(riter);

	ret = pmemstream_region_iterator_is_valid(riter);
	UT_ASSERTeq(ret, 0);

	pmemstream_region_iterator_next(riter);

	pmemstream_region_iterator_delete(&riter);
	UT_ASSERTeq(riter, NULL);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

// void valid_recovery_test(char *path)
// {
//	pmemstream_test_env env = pmemstream_test_make_default(path);

//	struct pmemstream_region_iterator *riter;

//	struct pmemstream_region region;
//	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_MULTI_SIZE, &region);
//	UT_ASSERTeq(ret, 0);

//	ret = pmemstream_region_iterator_new(&riter, env.stream);
//	UT_ASSERTeq(ret, 0);
//	UT_ASSERTne(riter, NULL);

//	ret = pmemstream_region_iterator_seek_first(riter);
//	UT_ASSERTeq(ret, 0);

//	ret = pmemstream_region_iterator_next(riter);
//	UT_ASSERTeq(ret, -1);

//	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_MULTI_SIZE, &region);
//	UT_ASSERTeq(ret, 0);

//	ret = pmemstream_region_iterator_next(riter);
//	UT_ASSERTeq(ret, 0);

//	ret = pmemstream_region_iterator_next(riter);
//	UT_ASSERTeq(ret, -1);

//	pmemstream_region_iterator_delete(&riter);
//	UT_ASSERTeq(riter, NULL);

//	pmemstream_region_free(env.stream, region);
//	pmemstream_test_teardown(env);
// }

void null_iterator_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_region_iterator_new(NULL, env.stream);
	UT_ASSERTeq(ret, -1);

	ret = pmemstream_region_iterator_is_valid(NULL);
	UT_ASSERTeq(ret, -1);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void invalid_region_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	const uint64_t invalid_offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes));
	struct pmemstream_region_iterator *riter = NULL;
	struct pmemstream_region invalid_region = {.offset = invalid_offset};

	int ret = pmemstream_region_iterator_new(&riter, env.stream);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(riter, NULL);

	pmemstream_region_iterator_seek_first(riter);

	invalid_region = pmemstream_region_iterator_get(riter);
	UT_ASSERTeq(invalid_region.offset, SLIST_INVALID_OFFSET);

	pmemstream_region_iterator_delete(&riter);
	pmemstream_test_teardown(env);
}

void null_stream_test()
{
	struct pmemstream_region_iterator *riter = NULL;
	int ret;

	ret = pmemstream_region_iterator_new(&riter, NULL);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(riter, NULL);

	pmemstream_region_iterator_delete(&riter);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}

	START();

	char *path = argv[1];

	valid_input_test(path);
	null_iterator_test(path);
	invalid_region_test(path);
	// valid_recovery_test(path);
	null_stream_test();

	return 0;
}
