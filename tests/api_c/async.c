// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "span.h"
#include "stream_helpers.h"
#include "unittest.h"

#include <libminiasync.h>
#include <string.h>

/**
 * async.c - unit test for pmemstream_async_publish, pmemstream_async_append,
 *		pmemstream_async_wait_committed, pmemstream_async_wait_persisted
 */

/* helper functions and structs */
struct entry_data {
	uint64_t data;
};

void valid_input_helper(char *path, bool do_memcpy)
{
	void *data_address = NULL;
	struct pmemstream_entry entry;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_region region;

	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	/* async append */
	struct data_mover_threads *dmt = data_mover_threads_default();
	UT_ASSERTne(dmt, NULL);
	struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);

	ret = pmemstream_async_append(env.stream, thread_mover, region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, 0);

	/* async reserve-publish */
	ret = pmemstream_reserve(env.stream, region, NULL, 0, &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	if (do_memcpy)
		memcpy(&data_address, &data, sizeof(data));

	ret = pmemstream_async_publish(env.stream, region, NULL, entry, sizeof(data));
	UT_ASSERTeq(ret, 0);

	/* async waits */
	struct pmemstream_async_wait_fut future1 = pmemstream_async_wait_committed(env.stream, 1);
	UT_ASSERTeq(future1.data.timestamp, 1);
	UT_ASSERTeq(future1.output.error_code, 0);
	while (future_poll(FUTURE_AS_RUNNABLE(&future1), NULL) != FUTURE_STATE_COMPLETE)
		;
	UT_ASSERTeq(future1.output.error_code, 0);

	struct pmemstream_async_wait_fut future2 = pmemstream_async_wait_persisted(env.stream, 2);
	UT_ASSERTeq(future2.data.timestamp, 2);
	UT_ASSERTeq(future2.output.error_code, 0);
	while (future_poll(FUTURE_AS_RUNNABLE(&future2), NULL) != FUTURE_STATE_COMPLETE)
		;
	UT_ASSERTeq(future2.output.error_code, 0);

	data_mover_threads_delete(dmt);
	pmemstream_test_teardown(env);
}

/* tests' functions */
void valid_input_test(char *path)
{
	bool do_memcpy = false;
	valid_input_helper(path, do_memcpy);
}

void valid_input_with_memcpy_test(char *path)
{
	bool do_memcpy = true;
	valid_input_helper(path, do_memcpy);
}

void null_stream_test(char *path)
{
	void *data_address = NULL;
	struct pmemstream_entry entry;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_region region;

	pmemstream_test_env env = pmemstream_test_make_default(path);

	int ret = pmemstream_region_allocate(env.stream, TEST_DEFAULT_REGION_SIZE, &region);
	UT_ASSERTeq(ret, 0);

	/* async append */
	struct data_mover_threads *dmt = data_mover_threads_default();
	UT_ASSERTne(dmt, NULL);
	struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);

	ret = pmemstream_async_append(NULL, thread_mover, region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, -1);

	/* async reserve-publish */
	ret = pmemstream_reserve(env.stream, region, NULL, 0, &entry, &data_address);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(data_address, NULL);

	ret = pmemstream_async_publish(NULL, region, NULL, entry, sizeof(data));
	UT_ASSERTeq(ret, -1);

	/* async waits */
	struct pmemstream_async_wait_fut future1 = pmemstream_async_wait_committed(NULL, 1);
	UT_ASSERTeq(future1.data.timestamp, 1);
	UT_ASSERTeq(future1.output.error_code, -1);
	while (future_poll(FUTURE_AS_RUNNABLE(&future1), NULL) != FUTURE_STATE_COMPLETE)
		;
	UT_ASSERTeq(future1.output.error_code, -1);

	struct pmemstream_async_wait_fut future2 = pmemstream_async_wait_persisted(NULL, 2);
	UT_ASSERTeq(future2.data.timestamp, 2);
	UT_ASSERTeq(future2.output.error_code, -1);
	while (future_poll(FUTURE_AS_RUNNABLE(&future2), NULL) != FUTURE_STATE_COMPLETE)
		;
	UT_ASSERTeq(future2.output.error_code, -1);

	data_mover_threads_delete(dmt);
	pmemstream_test_teardown(env);
}

void invalid_region_test(char *path)
{
	void *data_address = NULL;
	struct pmemstream_entry entry;
	struct entry_data data = {.data = PTRDIFF_MAX};
	struct pmemstream_region invalid_region = {.offset = ALIGN_DOWN(UINT64_MAX, sizeof(span_bytes))};

	pmemstream_test_env env = pmemstream_test_make_default(path);

	/* async append */
	struct data_mover_threads *dmt = data_mover_threads_default();
	UT_ASSERTne(dmt, NULL);
	struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);

	int ret = pmemstream_async_append(env.stream, thread_mover, invalid_region, NULL, &data, sizeof(data), &entry);
	UT_ASSERTeq(ret, -1);

	/* async reserve-publish */
	ret = pmemstream_reserve(env.stream, invalid_region, NULL, 0, &entry, &data_address);
	UT_ASSERTeq(ret, -1);

	ret = pmemstream_async_publish(env.stream, invalid_region, NULL, entry, sizeof(data));
	UT_ASSERTeq(ret, -1);

	data_mover_threads_delete(dmt);
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
	valid_input_with_memcpy_test(path);
	null_stream_test(path);
	invalid_region_test(path);

	return 0;
}
