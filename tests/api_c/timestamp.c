// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "libpmemstream.h"
#include "libpmemstream_internal.h"
#include "stream_helpers.h"
#include "unittest.h"

#include "string.h"

void check_timestamp_and_order(char *path)
{
	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret;
	uint64_t timestamp;
	struct pmemstream_entry entry = {.offset = PMEMSTREAM_INVALID_OFFSET};
	struct pmemstream_region region;
	const char *data = "some_entry_data";

	ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);
	for (size_t i = 0; i < 10; i++) {
		ret = pmemstream_append(env.stream, region, NULL, data, strlen(data), &entry);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTne(entry.offset, PMEMSTREAM_INVALID_OFFSET);

		timestamp = pmemstream_entry_timestamp(env.stream, entry);
		UT_ASSERTne(timestamp, PMEMSTREAM_INVALID_TIMESTAMP);
		if (i > 0) {
			UT_ASSERTeq(timestamp, PMEMSTREAM_FIRST_TIMESTAMP + i);
		}
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
	check_timestamp_and_order(path);

	return 0;
}
