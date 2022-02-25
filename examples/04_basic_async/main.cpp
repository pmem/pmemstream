// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <cassert>
#include <cstdio>
#include <libminiasync.h>
#include <libpmem2.h>

#define EXAMPLE_ASYNC_COUNT 3

struct data_entry {
	uint64_t data;
};

/**
 * Show example usage of sync and async appends.
 */
int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	/* prepare stream and allocate or get a region */
	struct pmem2_map *map = example_map_open(argv[1], EXAMPLE_STREAM_SIZE);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	if (ret) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return ret;
	}

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, 4096, &region);
	if (ret == -1) {
		struct pmemstream_region_iterator *riter;
		ret = pmemstream_region_iterator_new(&riter, stream);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_region_iterator_new failed\n");
			return ret;
		}
		/* if allocate failed, we try to open existing region */
		ret = pmemstream_region_iterator_next(riter, &region);
		pmemstream_region_iterator_delete(&riter);

		if (ret == -1) {
			fprintf(stderr, "pmemstream_region_iterator_next found no regions\n");
			return ret;
		}
	}

	struct data_entry example_data;
	example_data.data = 1024;
	struct pmemstream_entry entry;

	/*
	 * Example synchronous (regular) append
	 */
	ret = pmemstream_append(stream, region, NULL, &example_data, sizeof(example_data), &entry);
	if (ret) {
		fprintf(stderr, "pmemstream_append failed\n");
		return ret;
	}
	const struct data_entry *read_data = (const struct data_entry *)pmemstream_entry_data(stream, entry);
	printf("regular append read data: %lu\n", read_data->data);

	/*
	 * Example asynchronous append, executed in the libminiasync runtime
	 */
	/* prepare environment and define async appends ("futures") */
	struct data_mover_threads *dmt = data_mover_threads_default();
	if (dmt == NULL) {
		fprintf(stderr, "Failed to allocate data mover.\n");
		return -1;
	}
	struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);

	struct pmemstream_async_append_fut *append_futures = (struct pmemstream_async_append_fut *)malloc(
		EXAMPLE_ASYNC_COUNT * sizeof(struct pmemstream_async_append_fut));
	assert(append_futures != NULL);

	for (int i = 0; i < EXAMPLE_ASYNC_COUNT; ++i) {
		append_futures[i] = pmemstream_async_append(stream, thread_mover, region, NULL, &example_data,
							    sizeof(example_data));

		if (append_futures[i].output.error_code != 0) {
			fprintf(stderr, "pmemstream_async_append (no. %d) failed\n", i);
			return append_futures[i].output.error_code;
		}
	}

	/* Now, execute these futures. */

	/* For simply scenarios just run and wait for multiple futures: */
	// struct runtime *r = runtime_new();
	// runtime_wait_multiple(r, futures, EXAMPLE_ASYNC_COUNT);

	/* ... or alternatively manually poll until completion. */
	int completed_futures[EXAMPLE_ASYNC_COUNT] = {0};
	int completed = 0;
	do {
		for (int i = 0; i < EXAMPLE_ASYNC_COUNT; i++) {
			if (!completed_futures[i] &&
			    future_poll(FUTURE_AS_RUNNABLE(&append_futures[i]), NULL) == FUTURE_STATE_COMPLETE) {
				completed_futures[i] = 1;
				completed++;
			}
		}

		/*
		 * an additional user/application work could be done here
		 */
	} while (completed < EXAMPLE_ASYNC_COUNT);

	/* finally, read out the data of one of the async appends and print appended value */
	struct pmemstream_async_append_output *out = FUTURE_OUTPUT(&append_futures[0]);
	if (out->error_code != 0) {
		fprintf(stderr, "pmemstream_append_async failed\n");
		return ret;
	}
	read_data = (const struct data_entry *)pmemstream_entry_data(stream, out->new_entry);
	printf("async append (no. 0) read data: %lu\n", read_data->data);

	/* cleanup */
	free(append_futures);
	data_mover_threads_delete(dmt);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
