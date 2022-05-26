// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "common/util.h"
#include "span.h"
#include "stream_helpers.h"
#include "unittest.h"

/**
 * region_create - unit test for pmemstream_region_allocate, pmemstream_region_free,
 *					pmemstream_region_size, pmemstream_region_usable_size,
 *					pmemstream_region_runtime_initialize
 */

void valid_input_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	size_t region_size = pmemstream_region_size(env.stream, region);
	UT_ASSERT(region_size >= TEST_DEFAULT_REGION_SIZE);
	UT_ASSERT(pmemstream_region_usable_size(env.stream, region) < region_size);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_region_runtime_initialize(env.stream, region, &rtm);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(rtm, NULL);

	ret = pmemstream_region_free(env.stream, region);
	UT_ASSERTeq(ret, 0);

	pmemstream_test_teardown(env);
}

void null_stream_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	struct pmemstream_region region;
	ret = pmemstream_region_allocate(NULL, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, -1);

	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq(pmemstream_region_size(NULL, region), 0);
	UT_ASSERT(pmemstream_region_usable_size(NULL, region) <= TEST_DEFAULT_STREAM_SIZE);

	struct pmemstream_region_runtime *rtm = NULL;
	ret = pmemstream_region_runtime_initialize(NULL, region, &rtm);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(rtm, NULL);

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

void zero_size_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, 0, &region);
	UT_ASSERTeq(ret, -1);

	pmemstream_test_teardown(env);
}

void invalid_region_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};
	UT_ASSERT(pmemstream_region_size(env.stream, invalid_region) == 0);
	UT_ASSERT(pmemstream_region_usable_size(env.stream, invalid_region) == 0);

	struct pmemstream_region_runtime *rtm = NULL;
	int ret = pmemstream_region_runtime_initialize(env.stream, invalid_region, &rtm);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(rtm, NULL);

	ret = pmemstream_region_free(env.stream, invalid_region);
	UT_ASSERTeq(ret, -1);

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
	null_stream_test(path);
	zero_size_test(path);
	invalid_region_test(path);

	return 0;
}
