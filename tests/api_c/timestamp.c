// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/**
 * timestamp - unit test for pmemstream_entry_timestamp,
 *					pmemstream_committed_timestamp, pmemstream_persisted_timestamp
 */

#include "libpmemstream.h"
#include "libpmemstream_internal.h"
#include "stream_helpers.h"
#include "unittest.h"

#include <string.h>

void null_stream_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	uint64_t data = 128;
	struct pmemstream_entry entry;

	struct pmemstream_region region;
	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	ret = pmemstream_append(NULL, region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, -1);

	uint64_t timestamp = pmemstream_entry_timestamp(NULL, entry);
	UT_ASSERTeq(timestamp, PMEMSTREAM_INVALID_TIMESTAMP);

	timestamp = pmemstream_committed_timestamp(NULL);
	UT_ASSERTeq(timestamp, PMEMSTREAM_INVALID_TIMESTAMP);

	timestamp = pmemstream_persisted_timestamp(NULL);
	UT_ASSERTeq(timestamp, PMEMSTREAM_INVALID_TIMESTAMP);

	pmemstream_test_teardown(env);
}

void invalid_entry_test(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	struct pmemstream_entry invalid_entry = {.offset = UINT64_MAX};

	uint64_t timestamp = pmemstream_entry_timestamp(env.stream, invalid_entry);
	UT_ASSERTeq(timestamp, PMEMSTREAM_INVALID_TIMESTAMP);

	pmemstream_test_teardown(env);
}

void check_timestamp_and_order(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);
	struct pmemstream_region region = {.offset = PMEMSTREAM_INVALID_OFFSET};
	const char *data = "some_entry_data";

	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(region.offset, PMEMSTREAM_INVALID_OFFSET);

	for (size_t i = 0; i < 10; i++) {
		struct pmemstream_entry entry = {.offset = PMEMSTREAM_INVALID_OFFSET};
		uint64_t timestamp = PMEMSTREAM_INVALID_TIMESTAMP;
		uint64_t committed_timestamp = PMEMSTREAM_INVALID_TIMESTAMP;
		uint64_t persisted_timestamp = PMEMSTREAM_INVALID_TIMESTAMP;

		ret = pmemstream_append(env.stream, region, NULL, data, strlen(data), &entry);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTne(entry.offset, PMEMSTREAM_INVALID_OFFSET);

		timestamp = pmemstream_entry_timestamp(env.stream, entry);
		UT_ASSERTne(timestamp, PMEMSTREAM_INVALID_TIMESTAMP);
		UT_ASSERTeq(timestamp, PMEMSTREAM_FIRST_TIMESTAMP + i);

		committed_timestamp = pmemstream_committed_timestamp(env.stream);
		UT_ASSERTne(committed_timestamp, PMEMSTREAM_INVALID_TIMESTAMP);
		UT_ASSERTeq(timestamp, committed_timestamp);

		persisted_timestamp = pmemstream_persisted_timestamp(env.stream);
		UT_ASSERTne(persisted_timestamp, PMEMSTREAM_INVALID_TIMESTAMP);
		UT_ASSERTeq(timestamp, persisted_timestamp);
	}

	pmemstream_region_free(env.stream, region);
	pmemstream_test_teardown(env);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}
	char *path = argv[1];

	START();

	null_stream_test(path);
	invalid_entry_test(path);
	check_timestamp_and_order(path);

	return 0;
}
